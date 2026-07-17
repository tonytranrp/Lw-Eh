// Minimal but real exercise of the library through the find_package(LwEh)
// consumption path -- not just "it includes and links", but "it actually
// dispatches correctly" through both storage policies, mirroring the
// correctness the in-repo tests/ suite checks via add_subdirectory instead.
// Exit code is the pass/fail signal CI checks (see ../.github/workflows/ci.yml).
#include <lweh/lweh.hpp>
#include <cstdio>

struct ping_event {
    int value;
};

int g_free_hits = 0;
void on_ping(const ping_event& e) {
    g_free_hits += e.value;
}

struct listener {
    int hits = 0;
    void on_ping(const ping_event& e) { hits += e.value; }
};

struct node_listener : lweh::intrusive_node<ping_event> {
    bool fired = false;
    void on_event(const ping_event&) override { fired = true; }
};

int main() {
    lweh::signal<ping_event, 4> sig;
    listener l;
    sig.attach<&on_ping>();
    sig.attach<&listener::on_ping>(&l);
    sig.publish(ping_event{5});

    lweh::intrusive_signal<ping_event> isig;
    node_listener n;
    isig.attach(n);
    isig.publish(ping_event{1});

    bool ok = (g_free_hits == 5) && (l.hits == 5) && n.fired;
    std::printf(ok ? "OK: find_package(LwEh) consumption works end-to-end\n"
                    : "FAIL: unexpected dispatch result\n");
    return ok ? 0 : 1;
}
