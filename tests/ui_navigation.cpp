// DOM/event adapters for the actual navigation producer and consumer. No GPU
// or RmlUi layout is simulated: rectangles/visibility are explicit test inputs.
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace Rml {
struct Vector2f {
    float x = 0, y = 0;
    Vector2f operator+(Vector2f rhs) const { return {x + rhs.x, y + rhs.y}; }
};
struct Box { Vector2f GetSize() const { return {10, 10}; } };
namespace Style { enum class Focus { Auto, None }; enum class TabIndex { None, Auto }; }
struct ComputedValues {
    bool focusable = false;
    Style::Focus focus() const { return Style::Focus::Auto; }
    Style::TabIndex tab_index() const { return focusable ? Style::TabIndex::Auto : Style::TabIndex::None; }
};
struct Element {
    bool visible = true;
    Vector2f position;
    ComputedValues computed;
    Vector2f GetAbsoluteOffset() const { return position; }
    Box GetBox() const { return {}; }
    bool IsVisible() const { return visible; }
    const ComputedValues& GetComputedValues() const { return computed; }
};
namespace Input { enum KeyIdentifier { KI_UNKNOWN, KI_LEFT, KI_RIGHT, KI_UP, KI_DOWN }; }
enum class EventId { Keydown, Other };
enum class EventPhase { Bubble, Capture };
struct Event {
    int key;
    bool propagating = true;
    EventId id = EventId::Keydown;
    EventPhase phase = EventPhase::Bubble;
    EventId GetId() const { return id; }
    EventPhase GetPhase() const { return phase; }
    bool IsPropagating() const { return propagating; }
    template<class T> T GetParameter(const char*, T) const { return static_cast<T>(key); }
    void StopPropagation() { propagating = false; }
};
}
namespace recompui {
enum class NavigationType { Auto, Horizontal, Vertical, GridRow, GridCol };
enum class NavDirection { Up, Down, Left, Right };
struct Element;
static Element* focused = nullptr;
struct Element {
    enum class CanFocus { Yes, No, NoAndNoChildren };
    Rml::Element dom;
    Rml::Element* base = &dom;
    Element* parent = nullptr;
    std::vector<Element*> children, nav_children;
    bool enabled = true, is_nav_container = false, is_nav_wrapping = false, is_primary_focus = false;
    NavigationType nav_type = NavigationType::Auto;
    Element* get_nav_parent();
    CanFocus is_focusable();
    void get_all_focusable_children(Element*);
    void build_navigation(Element*, Element*);
    bool focus() { focused = this; return true; }
};
struct Document : Element {
    bool handle_navigation_event(Rml::Event&);
    void report_focused_element() {}
};
struct Context { Element* get_focused_element() { return focused; } };
static Context get_current_context() { return {}; }

#include "navigation_producer.inc"
#include "navigation_consumer.inc"
}

using namespace recompui;
#define CHECK(x) do { if (!(x)) throw std::runtime_error("FAILED: " #x); } while (0)

static void attach(Element& parent, Element& child, bool container, bool focusable, float x, float y) {
    child.parent = &parent;
    parent.children.push_back(&child);
    child.is_nav_container = container;
    child.dom.computed.focusable = focusable;
    child.dom.position = {x, y};
}

// Check the actual rebuilt navigation edges, including all ancestor membership.
static bool active(Element* root, Element* element) {
    for (auto* child : root->nav_children) {
        if (child == element || active(child, element)) return true;
    }
    return false;
}

static void normal_radio() {
    Document doc;
    doc.is_nav_container = true; doc.nav_type = NavigationType::Vertical;
    Element row, above, below, none, two, four;
    attach(doc, above, false, true, 0, 0);
    attach(doc, row, true, false, 0, 20); row.nav_type = NavigationType::Horizontal;
    attach(doc, below, false, true, 0, 40);
    attach(row, none, false, true, 0, 20);
    attach(row, two, false, true, 20, 20);
    attach(row, four, false, true, 40, 20);
    four.is_primary_focus = true;
    focused = &four;
    for (auto [key, target] : std::vector<std::pair<int, Element*>>{
            {Rml::Input::KI_LEFT, &two}, {Rml::Input::KI_LEFT, &none},
            {Rml::Input::KI_RIGHT, &two}, {Rml::Input::KI_RIGHT, &four},
            {Rml::Input::KI_DOWN, &below}, {Rml::Input::KI_UP, &four},
            {Rml::Input::KI_UP, &above}}) {
        Rml::Event e{key};
        CHECK(doc.handle_navigation_event(e));
        CHECK(focused == target && active(&doc, focused) && !e.propagating);
    }
    row.is_nav_wrapping = true; focused = &four;
    Rml::Event right{Rml::Input::KI_RIGHT}; doc.handle_navigation_event(right);
    CHECK(focused == &none);
    Rml::Event left{Rml::Input::KI_LEFT}; doc.handle_navigation_event(left);
    CHECK(focused == &four);
}

// A page can be excluded while RmlUi still reports its previous focused leaf.
// Seed the old lists with the real producer before pruning that page. The old
// consumer walks stale physical parents and throws at the absent page edge.
static void pruned_focus(bool hidden, int key) {
    Document doc;
    doc.is_nav_container = true; doc.nav_type = NavigationType::Vertical;
    Element retired, current, old_row, old_two, old_four, replacement;
    attach(doc, retired, true, false, 0, 0); retired.nav_type = NavigationType::Vertical;
    attach(doc, current, true, false, 0, 40); current.nav_type = NavigationType::Vertical;
    attach(retired, old_row, true, false, 0, 0); old_row.nav_type = NavigationType::Horizontal;
    attach(old_row, old_two, false, true, 0, 0);
    attach(old_row, old_four, false, true, 20, 0);
    attach(current, replacement, false, true, 0, 40);
    focused = &old_two;
    doc.build_navigation(&doc, focused);
    CHECK(active(&doc, focused));
    if (hidden) retired.dom.visible = false;
    else retired.enabled = false;
    Rml::Event e{key};
    CHECK(doc.handle_navigation_event(e));
    CHECK(focused == &replacement && active(&doc, focused) && !e.propagating);
    // Recovery must leave navigation usable when the old page returns.
    retired.dom.visible = true; retired.enabled = true;
    Rml::Event up{Rml::Input::KI_UP}; doc.handle_navigation_event(up);
    CHECK(active(&doc, focused) && (focused == &old_two || focused == &old_four));
}

static void empty_and_filtered() {
    Document doc; doc.is_nav_container = true;
    Element retired; attach(doc, retired, false, true, 0, 0);
    // A detached but still live focus reference has no edge in this document.
    doc.children.clear(); focused = &retired;
    Rml::Event left{Rml::Input::KI_LEFT};
    CHECK(doc.handle_navigation_event(left));
    CHECK(focused == &retired && !left.propagating);
    focused = nullptr;
    Rml::Event down{Rml::Input::KI_DOWN}; CHECK(!doc.handle_navigation_event(down));
    focused = &retired;
    Rml::Event unknown{Rml::Input::KI_UNKNOWN}; CHECK(!doc.handle_navigation_event(unknown));
    Rml::Event capture{Rml::Input::KI_DOWN}; capture.phase = Rml::EventPhase::Capture;
    CHECK(!doc.handle_navigation_event(capture));
}

static void grid_and_auto() {
    Document doc; doc.is_nav_container = true; doc.nav_type = NavigationType::GridCol;
    Element row0, row1, a, b, c, d;
    attach(doc, row0, true, false, 0, 0); row0.nav_type = NavigationType::GridRow;
    attach(doc, row1, true, false, 0, 20); row1.nav_type = NavigationType::GridRow;
    attach(row0, a, false, true, 0, 0); attach(row0, b, false, true, 20, 0);
    attach(row1, c, false, true, 0, 20); attach(row1, d, false, true, 20, 20);
    focused = &b;
    for (auto [key, target] : std::vector<std::pair<int, Element*>>{
            {Rml::Input::KI_DOWN, &d}, {Rml::Input::KI_UP, &b},
            {Rml::Input::KI_LEFT, &a}, {Rml::Input::KI_RIGHT, &b}}) {
        Rml::Event e{key}; CHECK(doc.handle_navigation_event(e)); CHECK(focused == target);
    }
    d.enabled = false;
    Rml::Event down{Rml::Input::KI_DOWN}; doc.handle_navigation_event(down); CHECK(focused == &c);

    Document positions; positions.is_nav_container = true; positions.nav_type = NavigationType::Auto;
    Element center, up, right, far;
    attach(positions, center, false, true, 0, 20);
    attach(positions, up, false, true, 0, 0);
    attach(positions, right, false, true, 20, 20);
    attach(positions, far, false, true, 40, 20);
    focused = &center;
    Rml::Event go_right{Rml::Input::KI_RIGHT}; positions.handle_navigation_event(go_right); CHECK(focused == &right);
    focused = &center;
    Rml::Event go_up{Rml::Input::KI_UP}; positions.handle_navigation_event(go_up); CHECK(focused == &up);
}

int main() {
    try {
        // Start with the recorded exception's missing-parent-edge condition.
        pruned_focus(true, Rml::Input::KI_DOWN);
        for (bool hidden : {false, true}) {
            for (int key : {Rml::Input::KI_LEFT, Rml::Input::KI_RIGHT, Rml::Input::KI_UP, Rml::Input::KI_DOWN}) {
                pruned_focus(hidden, key);
            }
        }
        normal_radio();
        empty_and_filtered();
        grid_and_auto();
        std::cout << "PASS: actual navigation producer/consumer recover pruned focus, preserve radio/row/wrap movement and filter non-navigation events\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
