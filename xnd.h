using namespace game20x;

constexpr int DATA_LEN = 1024 * 1024;
VType data[DATA_LEN];

template<int N>
struct XNAction {
    static constexpr int MAX_HASH = N;

    int a;
    int hash() const { return a; }
    string dumps() const { return to_string(a); }

    XNAction(int ta = 0) : a(ta) {}

    bool operator== (const XNAction& other) const { return a == other.a; }
};

template<int N, int D>
struct XNDState {
    static void SetSingleData(int r, int c, int v) {
        ASSERT(D == 1);
        data[r * N + c] = v;
    }

    using Action = XNAction<N>;

    int n;
    int d;
    int a0[D], a1[D];

    XNDState() : n(N) {}

    void setN(int n) { this->n = n; }

    bool isTerminal() const { return d == D; }
    VType estimateU(bool needsDupm = false) const {
        if (d != D) {
            return 0;
        }
        // a0[0] a1[0] a0[1] a1[1] ...
        int index = 0;
        for(int i = 0; i < D; ++i) {
            index *= N * N;
            index += a0[i] * N + a1[i];
        }
        // if (a0[0] == 0 && a1[0] == 17) { // TODO TEST
        //     cerr << "index :" << index << ", data[index]: " << data[index] << endl;
        // }
        return data[index];
    }

    XNDState next(const Action& a0, const Action& a1) const {
        XNDState result = *this;
        result.d++;
        result.a0[d] = a0.a;
        result.a1[d] = a1.a;
        return result;
    }

    int getAllActionCount(Player player) const { return n; }
    vector<Action> getAllActions(Player player) const {
        vector<Action> result;
        for(int i = 0; i < n; ++i)
            result.push_back(Action(i));
        return result;
    }
    Action sampleRandomAction(Player player) const { return Action(rand() % n); }


    string dumps() const {
        string result = to_string(d);
        for(int i = 0; i < d; ++i) {
            result += " a0[" + to_string(i) + "] = " + to_string(a0[d]);
            result += " a1[" + to_string(i) + "] = " + to_string(a1[d]);
        }
        return result;
    }

    static void DumpFormattedData(ostream& os = cout) {
        int len = 1;
        for(int i = 0; i < D; ++i)
            len *= N * N;
        for(int i = 0; i < len; ++i) {
            os << data[i] << ", ";
            if (i % N == N - 1)
                os << endl;
            if (i % (N * N) == N * N - 1)
                os << endl;
        }
    }

    static void DumpSingleLayeredData(ostream& os, int n) {
        for(int i = 0; i < n; ++i) {
            for(int j = 0; j < n; ++j)
                os << data[i * N + j] << ", \t";
            os << endl;
        }
    }
};
