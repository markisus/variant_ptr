Motivation
---------

In C++ we sometimes run into the following problem:

We have
- a set of types `As = { A1, A2, A3, ... An }`
- a set of types `Bs = { B1, B2, B3, ... Bm }`
- and a set of functions `Fs = { A1 -> B1, A1 -> B2, ... , A2 -> B1, A2 -> B2, ... }`

And in problem we are trying to solve, it would be very convenient if we could write the following code:

```c++

struct Fs {
    void call(A1 a, B1 b) { ... };
    void call(A1 a, B2 b) { ... };
    void call(A1 a, B3 b) { ... };
    void call(A1 a, Bm b) { ... };
    ...
    void call(An a, Bm b) { ... };
};

std::vector<As> my_as = { ... };
std::vector<Bs> my_bs = { ... };

for (auto& a : my_as) {
    for (auto& b : my_bs) {
        // some contraption that calls the correct function from Fs
        // depending on the combination of a and b
        Fs.call(a, b);
    }
}
```

Unfortunately, `As` and `Bs` are a union of types, which are not easily represented as a valid type in C++ and therefore `std::vector<As>` cannot be instantiated (and likewise with `Bs`). A common solution is to have all the `As` derive from some base class, and likewise with `Bs`, and have the `std::vectors` instantiated over pointers to the parent types.

```c++
std::vector<std::shared_ptr<ABase>> my_as = { ... };
std::vector<std::shared_ptr<BBase>> my_bs = { ... };

for (auto& a : my_as) {
    for (auto& b : my_bs) {
        // some contraption that calls the correct function from Fs
        // depending on the combination of a and b
        Fs.call(*a, *b);
    }
}
```

But this only solves half the problem. The `std::vector` has no type information about the child types, and now there is no way to dispatch `Fs.call(*a, *b)` since `*a` has type `ABase` and not one of the child types. So then we have to further pervert the code.
```c++
std::vector<std::shared_ptr<ABase>> my_as = { ... };
std::vector<std::shared_ptr<BBase>> my_bs = { ... };

for (auto& a : my_as) {
    for (auto& b : my_bs) {
        if (A1* a1 = dynamic_cast<A1*>(a.get())) {
            if (B1* b1 = dynamic_cast<B1*>(b.get())) {
                Fs.call(*a1, *b1);
            }
            else if ( ... ) {
                Fs.call(*a1, *bi);
            }
            ...
            else if (Bn* bn = dynamic_cast<Bn*>(b.get())) {
                Fs.call(*a1, *bn);
            }
            else {
                // someone added a new B without handling it
                throw;
            }
        }
        else if (A2* a2 = dynamic_cast<A2*>(a.get())) {
            if (B1* b1 = dynamic_cast<B1*>(b.get())) {
                Fs.call(*a2, *b1);
            }
            ...
            else {
               // someone added a new B without handling it
               throw;
            }
        }
        ...
        else if (An* an = dynamic_cast<An*>(a.get())) {
            if (B1* b1 = ...) {
    	    ...
    	}
        ...
        else {
            // someone added a new As without handling it
    	    throw;
        }
}
```

This is insanity. One possible solution is the usage of the classic [visitor pattern](https://en.wikipedia.org/wiki/Double_dispatch#Double_dispatch_in_C++ "visitor pattern") using inheritance.

This repository tries to implement a different approach using template meta-programming hackery.

Solution
--------
we can construct a templated type `variant_ptr<As...>` coupled with a function `apply_visitor(visitor, variant_ptr_a, variant_ptr_b, variant_c, ...)` which calls `visitor.visit(A a, B b, C c, ...)` where `A`, `B`, `C`, etc, are the underlying "child types" inside the `variant_ptr_a`, `variant_ptr_b`, `variant_ptr_c`, etc. Then we can write the following code:

```c++
std::vector<variant_ptr<A1, A2, ..., An>> my_as = { ... };
std::vector<variant_ptr<B1, B2, ..., Bm> my_bs = { ... };

for (auto& a : my_as) {
    for (auto& b : my_bs) {
        apply_visitor(Fs, a, b);
    }
}
```

Full example (main.cpp)
-------------

```c++
#include <iostream>
#include <vector>
#include "variant_ptr.h"

using namespace lius_tools;

struct Rock {};
struct Paper {};
struct Scissors {};

struct GetDescription {
  static std::string visit(Rock rock) {
    return "Rock, a very solid move.";
  }
  static std::string visit(Paper paper) {
    return "Paper, a very elusive move.";
  }
  static std::string visit(Scissors scissors) {
    return "Scissors, a very sharp move.";
  }
};

struct LosesTo {
  static bool visit(Rock rock, Paper paper) {
    return true;
  }

  static bool visit(Paper paper, Scissors scissors) {
    return true;
  }

  static bool visit(Scissors scissors, Rock rock) {
    return true;
  }

  template <typename A, typename B>
  static bool visit(A a, B b) {
    return false;
  }
};

using HandPtr = variant_ptr<Rock, Paper, Scissors>;

int main(int argc, char *argv[])
{
  Rock rock;
  Paper paper;
  Scissors scissors;

  LosesTo loses_to;
  GetDescription get_description;

  std::vector<HandPtr> alices_moves {
    HandPtr(&paper), HandPtr(&scissors), HandPtr(&paper), HandPtr(&rock)
  };
  std::vector<HandPtr> bobs_moves {
    HandPtr(&paper), HandPtr(&rock), HandPtr(&paper), HandPtr(&scissors)
  };

  for (size_t round_idx = 0; round_idx < alices_moves.size(); ++round_idx) {
    std::cout << "Round " << round_idx << "-----------" << std::endl;

    HandPtr alices_hand = alices_moves[round_idx];
    HandPtr bobs_hand = bobs_moves[round_idx];

    std::cout << "\tAlice throws " << alices_hand.visit(get_description) << std::endl;
    std::cout << "\tBob throws " << bobs_hand.visit(get_description) << std::endl;

    bool alice_loses_to_bob =
        apply_visitor(loses_to,
                      alices_hand,
                      bobs_hand);
    bool bob_loses_to_alice =
        apply_visitor(loses_to,
                      bobs_hand,
                      alices_hand);

    if (alice_loses_to_bob) {
      std::cout << "\tAlice loses to Bob" << std::endl;
    }
    else if (bob_loses_to_alice) {
      std::cout << "\tBob loses to Alice" << std::endl;
    }
    else {
      std::cout << "\tAlice and Bob tie" << std::endl;
    }
  }

  std::cout << "Game complete!" << std::endl;
  return 0;
}
```

Output:

```
Round 0-----------
        Alice throws Paper, a very elusive move.
        Bob throws Paper, a very elusive move.
        Alice and Bob tie
Round 1-----------
        Alice throws Scissors, a very sharp move.
        Bob throws Rock, a very solid move.
        Alice loses to Bob
Round 2-----------
        Alice throws Paper, a very elusive move.
        Bob throws Paper, a very elusive move.
        Alice and Bob tie
Round 3-----------
        Alice throws Rock, a very solid move.
        Bob throws Scissors, a very sharp move.
        Bob loses to Alice
Game complete!
```