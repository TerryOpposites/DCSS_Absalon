#ifndef RANDOM_H
#define RANDOM_H

#include <algorithm>  // iter_swap
#include <iterator>   // advance
#include <map>
#include <vector>

#include "hash.h"

void seed_rng();
void seed_rng(uint32_t seed);
void seed_rng(uint64_t[], int);

uint32_t get_uint32(int generator = RNG_GAMEPLAY);
uint64_t get_uint64(int generator = RNG_GAMEPLAY);
bool coinflip();
int div_rand_round(int num, int den);
int div_round_up(int num, int den);
bool one_chance_in(int a_million);
bool x_chance_in_y(int x, int y);
int random2(int max);
int maybe_random2(int x, bool random_factor);
int maybe_random_div(int nom, int denom, bool random_factor);
int maybe_roll_dice(int num, int size, bool random);
int random_range(int low, int high);
int random_range(int low, int high, int nrolls);
double random_real();

int random2avg(int max, int rolls);
int bestroll(int max, int rolls);
int biased_random2(int max, int n);
int random2limit(int max, int limit);
int binomial(unsigned n_trials, unsigned trial_prob, unsigned scale = 100);
bool bernoulli(double n_trials, double trial_prob);
int fuzz_value(int val, int lowfuzz, int highfuzz, int naverage = 2);
int roll_dice(int num, int size);
bool decimal_chance(double percent);

int ui_random(int max);

/** Chooses one of the objects passed in at random (by value).
 *  @return One of the arguments.
 */
template <typename T, typename... Ts>
T random_choose(T first, Ts... rest)
{
    const T elts[] = { first, rest... };
    return elts[random2(1 + sizeof...(rest))];
}

template <typename C>
auto random_iterator(C &container) -> decltype(container.begin())
{
    int pos = random2(container.size());
    auto it = container.begin();
    advance(it, pos);
    return it;
}

/**
 * Get a random weighted choice.
 *
 * Weights are assumed to be non-negative, but are allowed to be zero.
 * @tparam  V  A map, vector of pairs, etc., with the values of the
 *             map or the second elements of the pairs being integer
 *             weights.
 *
 * @param   choices  The collection of choice-weight pairs to choose from.
 *
 * @return  A pointer to the item in the chosen pair, or nullptr if all
 *          weights are zero. The pointer is const only if necessary.
 */
template <typename V>
auto random_choose_weighted(V &choices) -> decltype(&(begin(choices)->first))
{
    int total = 0;
    for (const auto &entry : choices)
        total += entry.second;
    int r = random2(total);
    int sum = 0;
    for (auto &entry : choices)
    {
        sum += entry.second;
        if (sum > r)
            return &entry.first;
    }
    return nullptr;
}

/**
 * Get an index for a random weighted choice using a fixed vector of
 * weights.
 *
 * Entries with a weight <= 0 are skipped.
 * @param choices The fixed vector with weights for each item.
 *
 * @return  A index corresponding to the selected item, or -1 if all
 *          weights were skipped.
 */
template <typename T, int SIZE>
int random_choose_weighted(const FixedVector<T, SIZE>& choices)
{
    int total = 0;
    for (auto weight : choices)
        if (weight > 0)
            total += weight;

    int r = random2(total);
    int sum = 0;
    for (int i = 0; i < SIZE; ++i)
    {
        if (choices[i] <= 0)
            continue;

        sum += choices[i];
        if (sum > r)
            return i;
    }
    return -1;
}

template <typename T>
T random_choose_weighted(int weight, T first, ...)
{
    va_list args;
    va_start(args, first);
    T chosen = first;
    int cweight = weight, nargs = 100;

    while (nargs-- > 0)
    {
        const int nweight = va_arg(args, int);
        if (!nweight)
            break;

        const int choice = va_arg(args, int);
        if (random2(cweight += nweight) < nweight)
            chosen = static_cast<T>(choice);
    }

    va_end(args);
    ASSERT(nargs > 0);

    return chosen;
}

const char* random_choose_weighted(int weight, const char* first, ...);

struct dice_def
{
    int num;
    int size;

    dice_def() : num(0), size(0) {}
    dice_def(int n, int s) : num(n), size(s) {}
    int roll() const;
};

dice_def calc_dice(int num_dice, int max_damage);

// I must be a random-access iterator.
template <typename I>
void shuffle_array(I begin, I end)
{
    size_t n = end - begin;
    while (n > 1)
    {
        const int i = random2(n);
        n--;
        iter_swap(begin + i, begin + n);
    }
}

template <typename T>
void shuffle_array(T &vec)
{
    shuffle_array(begin(vec), end(vec));
}

template <typename T>
void shuffle_array(T *arr, int n)
{
    shuffle_array(arr, arr + n);
}

/**
 * A defer_rand object represents an infinite tree of random values, allowing
 * for a much more functional approach to randomness.  defer_rand values which
 * have been used should not be copy-constructed. Querying the same path
 * multiple times will always give the same result.
 *
 * An important property of defer_rand is that, except for rounding,
 * `float(r.random2(X)) / X == float(r.random2(Y)) / Y` for all `X` and `Y`.
 * In other words:
 *
 * - The parameter you use on any given call does not matter.
 * - The object stores the fraction, not a specific integer.
 * - random2() is monotonic in its argument.
 *
 * Rephrased: The first time any node in the tree has a method called on
 * it, a random float between 0 and 1 (the fraction) is generated and stored,
 * and this float is combined with the method's parameters to arrive at
 * the result. Calling the same method on the same node with the same
 * parameters will always give the same result, while different parameters
 * or methods will give different results (though they'll all use the same
 * float which was generated by the first method call). Each node in the
 * tree has it's own float, so the same method+parameters on different
 * nodes will get different results.
 */
class defer_rand
{
    vector<uint32_t> bits;
    map<int, defer_rand> children;

    bool x_chance_in_y_contd(int x, int y, int index);
public:
    // TODO It would probably be a good idea to have some sort of random
    // number generator API, and the ability to pass RNGs into any function
    // that wants them.
    bool x_chance_in_y(int x, int y) { return x_chance_in_y_contd(x,y,0); }
    bool one_chance_in(int a_million) { return x_chance_in_y(1,a_million); }
    int random2(int maxp1);

    int random_range(int low, int high);
    int random2avg(int max, int rolls);

    defer_rand& operator[] (int i);
};

template<typename Iterator>
int choose_random_weighted(Iterator beg, const Iterator end)
{
    ASSERT(beg < end);

#ifdef DEBUG
    int times_set = 0;
#endif

    int totalweight = 0;
    int count = 0, result = 0;
    while (beg != end)
    {
        totalweight += *beg;
        if (random2(totalweight) < *beg)
        {
            result = count;
#ifdef DEBUG
            times_set++;
#endif
        }
        ++count;
        ++beg;
    }
#ifdef DEBUG
    ASSERT(times_set > 0);
#endif
    return result;
}

#endif
