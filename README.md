# Survey

* 각종 결정 사항들 투표 함 (일주일 정도 투표하고 결정함)
  * [이전 투표들](https://github.com/kimjoy2002/crawl/blob/master/SURVEY.md)
  * 최근 투표
    * 1.2.2 밸런스 투표 (11.01~) 투표 완료
      - 너머의 군단-적절8표(44.4%) 약함3표(16.7%) 너무약함1표(5.6%)
      - 아그라피드-적절3표(16.7%) 약함3표(16.7%) 사기2표(11.1%) 너무사기1표(5.6%) 너무약함1표(5.6%)
      - 와이트-적절6표(33.3%) 약함4표(22.2%) 너무약함3표(16.7%) 사기1표(5.6%) 너무사기1표(5.6%)
      - 와이트-적절7표(38.9%) 약함2표(11.1%) 사기1표(5.6%)
      - 웨폰마스터-적절12표(66.7%) 약함3표(16.7%) 너무약함1표(5.6%)
      - 천사-적절12표(66.7%) 사기3표(16.7%) 약함2표(11.1%) 너무약함1표(5.6%)
      - 위시스크롤-적절7표(38.9%) 사기4표(22.2%) 너무사기3표(16.7%)
      - 골렘제련소-조금과함5표(26.3%) 아주과함2표(10.5%) 적절2표(10.5%) 조금어려움1표(5.6%)    
    * 돌죽 0.26 적용 투표 (1.06~토너먼트끝날때까지) https://forms.gle/1swRzbUSHNvLmZi77 
      
# Design goals

* 삭제보단 컨텐츠 추가를 중심으로 개발을 할 예정임
  * 컨텐츠 추가에 대해서는 밸런스적인 문제가 보여도 우선 추가를 목표로 함 (선추가 후밸런싱)

* 현재 시스템을 유지하는 선에서 옛날 컨텐츠 복구도 주요 목표 중 하나임

* 일부 개선이나 추가를 하다보면 찬반이 갈릴 수 있는데 이 경우 투표를 진행할 예정임
  * 일주일정도 투표나온 결과로 진행함
  
* 나머지는 부담없이 자유롭게 진행하자

# Dungeon Crawl Stone Soup Korea Fork

[Dungeon Crawl Stone Soup](https://github.com/crawl/crawl/)의 포크

돌죽 0.24버전을 베이스로 시작함. 목표는 위에 디자인 골을 참고

누구나 참여가능한 돌죽 포크를 지향함

* 기여 방법
  1. Pull requests를 통한 기여
     * 이 브랜치를 Fork를 따서 작업후 Pull requests 요청하기
     * 빌드가 실패하는게 아니면 왠만해선 허용합니다.
     
     
  2. 직접 참가하기
     * pull request가 아니라도 어느정도 코딩할줄아는 수준이시면 레포지터리 권한 다 드림
     * 이렇게 권한을 받으면 직접 push해도 상관없음
     
    
  3. 기획 혹은 버그 신고
     * 코딩을 할 줄 몰라도 위에 Issues 메뉴에서 아이디어 제공, 혹은 버그 제보로 참여가능
     

# How to Build
  * 원본영어 판은 [링크](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/INSTALL.txt)를 참조 
  * 한글 해석 [링크](https://gall.dcinside.com/board/view/?id=rlike&no=261405)
    * [링크](https://github.com/kimjoy2002/crawl/issues/18) <- 빌드시 문제 발생하면 참조하기

# Develop Guide
  * 직업 만들기 [영어원문](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/docs/develop/background_creation.txt) [한글번역](https://gall.dcinside.com/board/view/?id=rlike&no=96789)
  * 신 만들기 [영어원문](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/docs/develop/god_creation.txt) [한글번역](https://github.com/kimjoy2002/crawl/issues/116)
  * 몬스터만들기 [영어원문](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/docs/develop/monster_creation.txt)
  * 변이 만들기 [영어원문](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/docs/develop/mutation_creation.txt)
  * 종족 만들기 [영어원문](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/docs/develop/species_creation.md)
  * 마법 만들기 [한글원문](https://gall.dcinside.com/board/view/?id=rlike&no=318987)

# ChangeLog
  
  [Link](https://github.com/kimjoy2002/crawl/blob/master/CHANGELOG.md)
  
# Webtile (Test Server)

* 웹타일주소
  *  http://joy1999.codns.com:8081/
  * 3시간마다 자동 업데이트함. push버전을 바로바로 테스트하고싶으면 이걸 이용
  * 플레이용도보단 테스트용도기때문에 위자드모드 가능한 버전도 열어뒀음
  * 주의) 기본적으로 서버용 컴퓨터가 아니라서 느림. 플레이용이 아닌 테스트용도로만 사용하세요

# Download

* [![Build Status](http://joy1999.codns.com:8080/buildStatus/icon?job=crawl%2Fcrawl)](http://joy1999.codns.com:8080/job/crawl/job/crawl/)

* Last Release (kimchi-1.2.4)
  * https://github.com/kimjoy2002/crawl/releases/tag/kimchi-1.2.4

* 타일판 다운로드
  * http://joy1999.codns.com:8999/download/stone_soup-kimchi-trunk-tiles-win64.zip - 64비트
  * http://joy1999.codns.com:8999/download/stone_soup-kimchi-trunk-tiles-win32.zip - 32비트
  * 매일 밤 새벽3시 자동 업데이트

* 콘솔판 다운로드
  * http://joy1999.codns.com:8999/download/stone_soup-kimchi-trunk-win64.zip - 64비트
  * http://joy1999.codns.com:8999/download/stone_soup-kimchi-trunk-win32.zip - 32비트
  * 매일 밤 새벽3시 자동 업데이트
  
* 이전 다운로드 파일들
  * http://joy1999.codns.com:8999/download/crawl/
  * 매일 자동빌드된 파일들이 올라감 너무 오래되면 삭제
