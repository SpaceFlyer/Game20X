#define DEBUG

#include "game.h"

#include <ctime>

using namespace game20x;

constexpr int DATA_LEN = 1024;
VType data[DATA_LEN]; // 4 2x2 games

template<int N>
struct XNAction {
    int a;
    int hash() const { return a; }
    string dumps() const { return to_string(a); }

    XNAction(int ta = 0) : a(ta) {}

    bool operator== (const XNAction& other) const { return a == other.a; }
};

template<int N, int D>
struct XNDState {
    using Action = XNAction<N>;

    int d;
    int a0[D], a1[D];

    int hash() const {
        int result = 0;
        for(int i = 0; i < d; ++i) {
            result *= N * N;
            result += a0[i] * N + a1[i];
        }
        return d + result * (D + 1);
    }

    bool operator== (const XNDState& other) const { return hash() == other.hash(); }

    bool isTerminal() const { return d == D; }
    VType estimateU() const {
        if (d != D) {
            return 0;
        }
        // a0[0] a1[0] a0[1] a1[1] ...
        int index = 0;
        for(int i = 0; i < D; ++i) {
            index *= N * N;
            index += a0[i] * N + a1[i];
        }
        return data[index];
    }

    XNDState next(const Action& a0, const Action& a1) const {
        XNDState result = *this;
        result.d++;
        result.a0[d] = a0.a;
        result.a1[d] = a1.a;
        return result;
    }

    int getAllActionCount(Player player) const { return N; }
    vector<Action> getAllActions(Player player) const {
        vector<Action> result;
        for(int i = 0; i < N; ++i)
            result.push_back(Action(i));
        return result;
    }
    Action sampleRandomAction(Player player) const { return Action(rand() % N); }


    string dumps() const {
        sprintf(gCharBuffer, "d = %d, a0 = %d, %d, a1 = %d, %d",
                d, a0[0], a0[1], a1[0], a1[1]);
        return gCharBuffer;
    }

    static void DumpFormattedData() {
        int len = 1;
        for(int i = 0; i < D; ++i)
            len *= N * N;
        for(int i = 0; i < len; ++i) {
            cout << data[i] << ", ";
            if (i % N == N - 1)
                cout << endl;
            if (i % (N * N) == N * N - 1)
                cout << endl;
        }
    }
};

using X2Action = XNAction<2>;
using X2State = XNDState<2, 2>;
using X2SearchNode = SearchNode<X2Action, X2State>;

using X3Action = XNAction<3>;
using X32State = XNDState<3, 2>;
using X3SearchNode = SearchNode<X3Action, X32State>;

// using XState = X2State;
// using XSearchNode = X2SearchNode;
using XState = X32State;
using XSearchNode = X3SearchNode;

int main() {
    srand(0);
    // for(int i = 0; i < 16; ++i) {
    //     data[i] = rand() % 17 - 8;
    //     if (i >= 4) {
    //         data[i] = data[i - 4]; // TODO TEST normal form
    //     }
    //     cout << data[i] << ' ';
    //     if (i % 2 == 1)
    //         cout << endl;
    //     if (i % 4 == 3)
    //         cout << endl;
    // }
    // cout << "data ended" << endl;

    for(int i = 0; i < DATA_LEN; ++i) {
        data[i] = rand() % 17 - 8;
    }

    XState::DumpFormattedData();

    XState initialState;
    initialState.d = 0;
    XSearchNode root(initialState);

    for(int t = 0; t < 10000; ++t) {
        root.visit(4); // depth limit is higher than terminal state depth
    }

    root.dump(cout, 0);

    return 0;
}
