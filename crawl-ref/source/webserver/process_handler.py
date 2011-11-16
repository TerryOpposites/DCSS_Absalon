import os, os.path, errno, fcntl
import subprocess
import datetime, time
import hashlib

import config

from tornado.escape import json_decode, json_encode

from terminal import TerminalRecorder
from connection import WebtilesSocketConnection
from util import DynamicTemplateLoader, dgl_format_str, parse_where_data
from game_data_handler import GameDataHandler

class CrawlProcessHandlerBase(object):
    def __init__(self, game_params, username, logger, io_loop):
        self.game_params = game_params
        self.username = username
        self.logger = logger
        self.io_loop = io_loop

        self.process = None
        self.client_path = self.config_path("client_path")
        self.where = {}
        self.wheretime = 0
        self.kill_timeout = None

        now = datetime.datetime.utcnow()
        self.formatted_time = now.strftime("%Y-%m-%d.%H:%M:%S")
        self.lock_basename = self.formatted_time + ".ttyrec"

        self.end_callback = None
        self._watchers = set()
        self._receivers = set()
        self.last_activity_time = time.time()

    def format_path(self, path):
        return dgl_format_str(path, self.username, self.game_params)

    def config_path(self, key):
        if key not in self.game_params: return None
        return self.format_path(self.game_params[key])

    def idle_time(self):
        return time.time() - self.last_activity_time

    def write_to_all(self, msg):
        for receiver in self._receivers:
            receiver.write_message(msg)

    def send_to_all(self, msg, **data):
        for receiver in self._receivers:
            receiver.send_message(msg, **data)

    def handle_process_end(self):
        if self.kill_timeout:
            self.io_loop.remove_timeout(self.kill_timeout)
            self.kill_timeout = None

        for watcher in list(self._watchers):
            watcher.stop_watching()

        if self.end_callback:
            self.end_callback()

    def update_watcher_description(self):
        watcher_names = [watcher.username for watcher in self._watchers
                         if watcher.username]
        anon_count = len(self._watchers) - len(watcher_names)
        s = ", ".join(watcher_names)
        if len(watcher_names) > 0 and anon_count > 0:
            s = s + ", and %i Anon" % anon_count
        elif anon_count > 0:
            s = "%i Anon" % anon_count
        self.send_to_all("update_spectators",
                         count = len(self._watchers),
                         names = s)

    def add_watcher(self, watcher, hide = False):
        if not hide:
            self._watchers.add(watcher)
        self._receivers.add(watcher)
        if self.client_path:
            self._send_client(watcher)
        self.update_watcher_description()

    def remove_watcher(self, watcher):
        if watcher in self._watchers:
            self._watchers.remove(watcher)
        self._receivers.remove(watcher)
        self.update_watcher_description()

    def watcher_count(self):
        return len(self._watchers)

    def send_client_to_all(self):
        for receiver in self._receivers:
            self._send_client(receiver)

    def _send_client(self, watcher):
        v = hashlib.sha1(os.path.abspath(self.client_path)).hexdigest()
        GameDataHandler.add_version(v,
                                    os.path.join(self.client_path, "static"))

        templ_path = os.path.join(self.client_path, "templates")
        loader = DynamicTemplateLoader.get(templ_path)
        templ = loader.load("game.html")
        game_html = templ.generate(version = v)
        watcher.send_message("game_client", content = game_html)

    def stop(self):
        self.process.send_signal(subprocess.signal.SIGHUP)
        t = time.time() + config.kill_timeout
        self.kill_timeout = self.io_loop.add_timeout(t, self.kill)

    def kill(self):
        if self.process:
            self.logger.info("Killing crawl process after SIGHUP did nothing.")
            self.process.send_signal(subprocess.signal.SIGTERM)
            self.kill_timeout = None

    def check_where(self):
        morgue_path = self.config_path("morgue_path")
        wherefile = os.path.join(morgue_path, self.username + ".where")
        try:
            if os.path.getmtime(wherefile) > self.wheretime:
                self.wheretime = time.time()
                with open(wherefile, "r") as f:
                    wheredata = f.read()

                if wheredata.strip() == "": return

                try:
                    newwhere = parse_where_data(wheredata)
                except:
                    self.logger.warn("Exception while trying to parse where file!",
                                     exc_info=True)
                else:
                    if (newwhere.get("status") == "active" or
                        newwhere.get("status") == "saved"):
                        self.where = newwhere
        except (OSError, IOError):
            pass

    def human_readable_where(self):
        try:
            return "L{xl} {char}, {place}".format(**self.where)
        except KeyError:
            return ""

    def _base_call(self):
        game = self.game_params

        call = [game["crawl_binary"],
                "-name",   self.username,
                "-rc",     os.path.join(self.config_path("rcfile_path"),
                                        self.username + ".rc"),
                "-macro",  os.path.join(self.config_path("macro_path"),
                                        self.username + ".macro"),
                "-morgue", self.config_path("morgue_path")]

        if "options" in game:
            call += game["options"]

        return call

    def note_activity(self):
        self.last_activity_time = time.time()

    def handle_input(self, msg):
        raise NotImplementedError()

class CrawlProcessHandler(CrawlProcessHandlerBase):
    def __init__(self, game_params, username, logger, io_loop):
        super(CrawlProcessHandler, self).__init__(game_params, username,
                                                  logger, io_loop)
        self.socketpath = None
        self.conn = None
        self.ttyrec_filename = None
        self.inprogress_lock = None
        self.inprogress_lock_file = None

        self._stale_pid = None
        self._stale_lockfile = None
        self._purging_timer = None
        self._process_hup_timeout = None

    def start(self):
        self._purge_locks_and_start(True)

    def _purge_locks_and_start(self, firsttime=False):
        # Purge stale locks
        lockfile = self._find_lock()
        if lockfile:
            try:
                with open(lockfile) as f:
                    pid = f.readline()
                pid = int(pid)
                self._stale_pid = pid
                self._stale_lockfile = lockfile
                if firsttime:
                    hup_wait = 10
                    self.send_to_all("stale_processes",
                                     timeout=hup_wait, game=self.game_params["name"])
                    to = self.io_loop.add_timeout(time.time() + hup_wait,
                                                  self._kill_stale_process)
                    self._process_hup_timeout = to
                else:
                    self._kill_stale_process()
            except:
                self.logger.error("Error while handling lockfile %s.", lockfile,
                                  exc_info=True)
                errmsg = ("Error while trying to terminate a stale process.<br>" +
                          "Please contact an administrator.")
                self.send_to_all("stale_process_fail", content=errmsg)
                self.handle_process_end()
        else:
            # No more locks, can start
            self._start_process()

    def _stop_purging_stale_processes(self):
        if not self._process_hup_timeout: return
        self.io_loop.remove_timeout(self._process_hup_timeout)
        self._stale_pid = None
        self._stale_lockfile = None
        self._purging_timer = None
        self._process_hup_timeout = None
        self.handle_process_end()

    def _find_lock(self):
        for path in os.listdir(self.config_path("inprogress_path")):
            if (path.startswith(self.username + ":") and
                path.endswith(".ttyrec")):
                return os.path.join(self.config_path("inprogress_path"),
                                    path)
        return None

    def _kill_stale_process(self, signal=subprocess.signal.SIGHUP):
        self._process_hup_timeout = None
        if signal == subprocess.signal.SIGHUP:
            self.logger.info("Purging stale lock at %s, pid %s.",
                             self._stale_lockfile, self._stale_pid)
        elif signal == subprocess.signal.SIGTERM:
            self.logger.warn("Terminating pid %s forcefully!",
                             self._stale_lockfile, self._stale_pid)
        try:
            os.kill(self._stale_pid, signal)
        except OSError, e:
            if e.errno == errno.ESRCH:
                # Process doesn't exist
                self._purge_stale_lock()
            else:
                self.logger.error("Error while killing process %s.", self._stale_pid,
                                  exc_info=True)
                errmsg = ("Error while trying to terminate a stale process.<br>" +
                          "Please contact an administrator.")
                self.send_to_all("stale_process_fail", content=errmsg)
                self.handle_process_end()
        else:
            if signal == subprocess.signal.SIGTERM:
                self._purge_stale_lock()
            else:
                if signal == subprocess.signal.SIGHUP:
                    self._purging_timer = 10
                else:
                    self._purging_timer -= 1

                if self._purging_timer > 0:
                    self.io_loop.add_timeout(time.time() + 1,
                                             self._check_stale_process)
                else:
                    self.logger.warn("Couldn't terminate pid %s gracefully.",
                                     self._stale_pid)
                    self.send_to_all("force_terminate?")

    def _check_stale_process(self):
        self._kill_stale_process(0)

    def _do_force_terminate(self, answer):
        if answer:
            self._kill_stale_process(subprocess.signal.SIGTERM)
        else:
            self.handle_process_end()

    def _purge_stale_lock(self):
        if os.path.exists(self._stale_lockfile):
            os.remove(self._stale_lockfile)

        self._purge_locks_and_start(False)

    def _start_process(self):
        self.socketpath = os.path.join(self.config_path("socket_path"),
                                       self.username + ":" +
                                       self.formatted_time + ".sock")

        try: # Unlink if necessary
            os.unlink(self.socketpath)
        except OSError, e:
            if e.errno != errno.ENOENT:
                raise

        game = self.game_params

        call = self._base_call() + ["-webtiles-socket", self.socketpath,
                                    "-await-connection"]

        ttyrec_path = self.config_path("ttyrec_path")
        self.ttyrec_filename = os.path.join(ttyrec_path, self.lock_basename)

        self.logger.info("Starting crawl.")

        self.process = TerminalRecorder(call, self.ttyrec_filename,
                                        self._ttyrec_id_header(),
                                        self.logger, self.io_loop)
        self.process.end_callback = self._on_process_end
        self.process.output_callback = self._on_process_output
        self.process.activity_callback = self.note_activity

        self.gen_inprogress_lock()

        self.conn = WebtilesSocketConnection(self.io_loop, self.socketpath)
        self.conn.message_callback = self._on_socket_message
        self.conn.connect()

        self.last_activity_time = time.time()

        self.check_where()

    def gen_inprogress_lock(self):
        self.inprogress_lock = os.path.join(self.config_path("inprogress_path"),
                                            self.username + ":" + self.lock_basename)
        f = open(self.inprogress_lock, "w")
        fcntl.lockf(f.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        self.inprogress_lock_file = f
        cols, lines = self.process.get_terminal_size()
        f.write("%s\n%s\n%s\n" % (self.process.pid, cols, lines))
        f.flush()

    def remove_inprogress_lock(self):
        fcntl.lockf(self.inprogress_lock_file.fileno(), fcntl.LOCK_UN)
        self.inprogress_lock_file.close()
        os.remove(self.inprogress_lock)

    def _ttyrec_id_header(self):
        clrscr = "\033[2J"
        crlf = "\r\n"
        templ = (clrscr + "\033[1;1H" + crlf +
                 "Player: %s" + crlf +
                 "Game: %s" + crlf +
                 "Server: %s" + crlf +
                 "Filename: %s" + crlf +
                 "Time: (%s) %s" + crlf +
                 clrscr)
        tstamp = int(time.time())
        ctime = time.ctime()
        return templ % (self.username, self.game_params["name"],
                        config.server_id, self.lock_basename,
                        tstamp, ctime)

    def _on_process_end(self):
        self.logger.info("Crawl terminated.")

        self.remove_inprogress_lock()

        self.process = None

        if self.conn:
            self.conn.close()
            self.conn = None

        self.handle_process_end()

    def add_watcher(self, watcher, hide = False):
        super(CrawlProcessHandler, self).add_watcher(watcher, hide)

        if self.conn:
            self.conn.send_message('{"msg":"spectator_joined"}')

    def handle_input(self, msg):
        obj = json_decode(msg)

        if obj["msg"] == "input" and self.process:
            self.last_action_time = time.time()

            data = ""
            for x in obj.get("data", []):
                data += chr(x)

            data += obj.get("text", u"").encode("utf8")

            self.process.write_input(data)

        elif obj["msg"] == "force_terminate":
            self._do_force_terminate(obj["answer"])

        elif obj["msg"] == "stop_stale_process_purge":
            self._stop_purging_stale_processes()

        elif self.conn:
            self.conn.send_message(msg.encode("utf8"))

    def _on_process_output(self, line):
        self.check_where()

        self.write_to_all(line)

    def _on_socket_message(self, msg):
        # stdout data is only used for compatibility to wrapper
        # scripts -- so as soon as we receive something on the socket,
        # we stop using stdout
        self.process.output_callback = None

        if msg.startswith("*"):
            # Special message to the server
            msg = msg[1:]
            msgobj = json_decode(msg)
            if msgobj["msg"] == "client_path":
                if self.client_path == None:
                    self.client_path = self.format_path(msgobj["path"])
                    self.send_client_to_all()
            else:
                self.logger.warn("Unknown message from the crawl process: %s",
                                 msgobj["msg"])
        else:
            self.check_where()

            self.write_to_all(msg)



class DGLLessCrawlProcessHandler(CrawlProcessHandler):
    def __init__(self, logger, io_loop):
        game_params = dict(
            name = "DCSS",
            ttyrec_path = "./",
            inprogress_path = "./",
            socket_path = "./",
            client_path = "./webserver/game_data")
        super(DGLLessCrawlProcessHandler, self).__init__(game_params,
                                                         "game",
                                                         logger, io_loop)

    def _base_call(self):
        return ["./crawl"]

    def check_where(self):
        pass


class CompatCrawlProcessHandler(CrawlProcessHandlerBase):
    def __init__(self, game_params, username, logger, io_loop):
        super(CompatCrawlProcessHandler, self).__init__(game_params, username,
                                                        logger, io_loop)
        self.client_path = game_params["client_prefix"]

    def start(self):
        game = self.game_params
        call = self._base_call()

        self.logger.info("Starting crawl (compat-mode).")

        self.process = subprocess.Popen(call,
                                        stdin = subprocess.PIPE,
                                        stdout = subprocess.PIPE,
                                        stderr = subprocess.PIPE)

        self.io_loop.add_handler(self.process.stdout.fileno(), self.on_stdout,
                                self.io_loop.READ | self.io_loop.ERROR)
        self.io_loop.add_handler(self.process.stderr.fileno(), self.on_stderr,
                                self.io_loop.READ | self.io_loop.ERROR)

        self.last_activity_time = time.time()

        self.create_mock_ttyrec()

        self.check_where()

    def create_mock_ttyrec(self):
        running_game_path = self.config_path("running_game_path")
        self.ttyrec_filename = os.path.join(running_game_path,
                                            self.username + ":" +
                                            self.lock_basename)
        f = open(self.ttyrec_filename, "w")
        f.close()

    def delete_mock_ttyrec(self):
        if self.ttyrec_filename:
            os.remove(self.ttyrec_filename)
            self.ttyrec_filename = None

    def poll_crawl(self):
        if self.process is not None and self.process.poll() is not None:
            self.io_loop.remove_handler(self.process.stdout.fileno())
            self.io_loop.remove_handler(self.process.stderr.fileno())
            self.process.stdout.close()
            self.process.stderr.close()
            self.process = None

            self.logger.info("Crawl terminated. (compat-mode)")

            self.delete_mock_ttyrec()
            self.handle_process_end()

    def add_watcher(self, watcher, hide = False):
        super(CompatCrawlProcessHandler, self).add_watcher(watcher, hide)

        if self.process:
            self.process.write_input("^r")

    def handle_input(self, msg):
        if msg.startswith("{"):
            obj = json_decode(msg)

            self.note_activity()

            if obj["msg"] == "input" and self.process:
                self.last_action_time = time.time()

                data = ""
                for x in obj.get("data", []):
                    data += chr(x)

                data += obj.get("text", u"").encode("utf8")

                if data == "^":
                    self.process.write_input("\\94\n")

                self.process.stdin.write(data)

            elif obj["msg"] == "key" and self.process:
                self.process.stdin.write("\\" + str(obj["keycode"]) + "\n")

        else:
            if not msg.startswith("^"):
                self.note_activity()
            self.process.stdin.write(msg.encode("utf8"))

    def on_stderr(self, fd, events):
        if events & self.io_loop.ERROR:
            self.poll_crawl()
        elif events & self.io_loop.READ:
            s = self.process.stderr.readline()

            if not (s.isspace() or s == ""):
                self.logger.info("ERR: %s", s.strip())

            self.poll_crawl()

    def on_stdout(self, fd, events):
        if events & self.io_loop.ERROR:
            self.poll_crawl()
        elif events & self.io_loop.READ:
            msg = self.process.stdout.readline()

            self.write_to_all(msg)

            self.poll_crawl()
            self.check_where()

    def _send_client(self, watcher):
        templ_path = os.path.join(config.template_path, self.client_path)
        loader = DynamicTemplateLoader.get(templ_path)
        templ = loader.load("game.html")
        game_html = templ.generate(prefix = self.client_path)
        watcher.send_message("game_client", content = game_html)
