#pragma once

// Platform-neutral accessibility semantics for WhatsUI controls.
//
// This header models the information a native accessibility bridge needs, and
// provides a deterministic tree snapshot for tests and diagnostics.  It does
// not register a Windows UI Automation provider, expose an OS accessibility
// tree, synthesize input, or promise screen-reader integration by itself.
// Platform backends can adapt AccessibilityNode snapshots to their native API.

#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "wui/types.h"

namespace wui {

enum class AccessibilityRole {
    Unknown,
    Application,
    Group,
    Heading,
    Text,
    Button,
    CheckBox,
    RadioButton,
    RadioGroup,
    Switch,
    Slider,
    TextField,
    List,
    ListItem,
    Menu,
    MenuItem,
    Dialog,
    ProgressBar,
    Alert,
    Image,
    Separator,
    Toolbar,
    TabList,
    Tab,
    TabPanel,
    Link,
    ComboBox,
    ListBox,
    Option,
    Calendar,
    Table,
    DataGrid,
    ColumnHeader,
    TableRow,
    TableCell,
    DataGridRow,
    DataGridCell,
    Tree,
    TreeItem,
};

enum class AccessibilityActionKind {
    Invoke,
    Toggle,
    Expand,
    Collapse,
    SetValue,
    SetFocus,
};

struct AccessibilityActionCapabilities {
    bool invoke{false};
    bool toggle{false};
    bool expandCollapse{false};
    bool setValue{false};
    bool focus{false};
    bool valueReadOnly{false};
};

enum class AccessibilityActionStatus {
    Succeeded,
    ElementNotAvailable,
    ElementNotEnabled,
    NotSupported,
    InvalidValue,
    WindowClosed,
    TimedOut,
    Failed,
};

// Semantic data belonging to one control or logical content node.  Labels and
// descriptions are plain UTF-8 text; value is deliberately optional because a
// Button normally has no value while a Slider or editable TextField does.
struct AccessibilityProperties {
    AccessibilityRole role{AccessibilityRole::Unknown};
    std::string label;
    std::string description;
    // Author-supplied, window-unique stable identity for native automation.
    // Dynamic/keyed collections should set this so an OS-retained element
    // survives reorder and row reconstruction. Empty falls back to the visual
    // snapshot path.
    std::string automationId;
    bool enabled{true};
    // Focus is a snapshot of the framework focus manager, rather than a
    // mutable property maintained by every individual widget.
    bool focused{false};
    // Logical client coordinates recorded during layout. Native adapters are
    // responsible for converting this rectangle to their required coordinate
    // system (for example, screen pixels for Windows UI Automation).
    std::optional<RectF> bounds;
    std::optional<bool> checked;
    // Tri-state checkboxes remain part of the Toggle pattern while exposing
    // the platform's indeterminate/partially-checked state. `checked` remains
    // present (and false) so existing two-state consumers stay compatible.
    bool mixed{false};
    // Native form semantics shared by labelled selection/range controls.
    bool required{false};
    // An indeterminate operation is busy but intentionally has no numeric
    // value. Platform adapters can surface this as their item/status property.
    bool busy{false};
    // A transient status message should be announced by screen readers without
    // taking focus. Native adapters project this as a polite live region.
    bool live{false};
    // Present only for controls exposing an expand/collapse relationship,
    // such as a MenuButton or the disclosure half of a SplitButton.
    std::optional<bool> expanded;
    // Hierarchical collections expose one-based item depth (for example a
    // TreeItem's UIA Level property). The absence of a value means the role
    // does not participate in a hierarchy.
    std::optional<int> level;
    std::optional<std::string> value;
    // Native range controls retain numeric data separately from their
    // localized/spoken value string so adapters never reparse display text.
    std::optional<double> numericValue;
    std::optional<double> minimumValue;
    std::optional<double> maximumValue;
    std::optional<double> smallChange;
    std::optional<double> largeChange;
    AccessibilityActionCapabilities actions{};

    [[nodiscard]] bool hasAccessibleName() const noexcept
    {
        return !label.empty();
    }

    [[nodiscard]] bool supportsCheckedState() const noexcept
    {
        return role == AccessibilityRole::CheckBox || role == AccessibilityRole::RadioButton ||
               role == AccessibilityRole::Switch;
    }
};

// Cross-thread native adapters enqueue this value-only request. The UI thread
// resolves path against the current active semantic tree and verifies the
// expected role/name before invoking a mutable Node.
struct AccessibilityActionRequest {
    AccessibilityActionKind kind{AccessibilityActionKind::Invoke};
    std::vector<std::size_t> path;
    AccessibilityRole expectedRole{AccessibilityRole::Unknown};
    std::string automationId;
    std::string expectedLabel;
    std::string value;
};

using AccessibilityActionHandler =
    std::function<AccessibilityActionStatus(const AccessibilityActionRequest&)>;

class Node;

// A semantic tree deliberately separated from the visual Node tree.  This
// keeps the core model usable in headless tests and lets a platform adapter
// decide how visual composition, overlays, and virtualization map to native
// accessibility objects.
class AccessibilityNode {
public:
    AccessibilityNode() = default;

    explicit AccessibilityNode(AccessibilityProperties properties)
        : properties_(std::move(properties))
    {
    }

    explicit AccessibilityNode(AccessibilityRole role)
    {
        properties_.role = role;
    }

    [[nodiscard]] const AccessibilityProperties& properties() const noexcept
    {
        return properties_;
    }

    [[nodiscard]] AccessibilityProperties& properties() noexcept
    {
        return properties_;
    }

    AccessibilityNode& setRole(AccessibilityRole role) noexcept
    {
        properties_.role = role;
        return *this;
    }

    AccessibilityNode& setLabel(std::string label)
    {
        properties_.label = std::move(label);
        return *this;
    }

    AccessibilityNode& setDescription(std::string description)
    {
        properties_.description = std::move(description);
        return *this;
    }

    AccessibilityNode& setAutomationId(std::string automationId)
    {
        properties_.automationId = std::move(automationId);
        return *this;
    }

    AccessibilityNode& setEnabled(bool enabled) noexcept
    {
        properties_.enabled = enabled;
        return *this;
    }

    // Checked is meaningful for toggles and selection controls. The model
    // allows callers to carry it on custom controls too; adapters can reject
    // unsupported combinations according to their platform's constraints.
    AccessibilityNode& setChecked(std::optional<bool> checked) noexcept
    {
        properties_.checked = checked;
        return *this;
    }

    AccessibilityNode& clearChecked() noexcept
    {
        properties_.checked.reset();
        return *this;
    }

    AccessibilityNode& setValue(std::optional<std::string> value)
    {
        properties_.value = std::move(value);
        return *this;
    }

    AccessibilityNode& setValue(std::string value)
    {
        properties_.value = std::move(value);
        return *this;
    }

    AccessibilityNode& clearValue() noexcept
    {
        properties_.value.reset();
        return *this;
    }

    AccessibilityNode& addChild(AccessibilityNode child)
    {
        children_.push_back(std::move(child));
        return children_.back();
    }

    [[nodiscard]] const std::vector<AccessibilityNode>& children() const noexcept
    {
        return children_;
    }

    [[nodiscard]] std::vector<AccessibilityNode>& children() noexcept
    {
        return children_;
    }

private:
    AccessibilityProperties properties_{};
    std::vector<AccessibilityNode> children_;
};

struct AccessibilitySnapshotEntry {
    // Child indices from the snapshot root. An empty path identifies root;
    // {2, 0} identifies the first child of root's third child.
    std::vector<std::size_t> path;
    std::size_t depth{0};
    AccessibilityProperties properties;
};

using AccessibilitySnapshot = std::vector<AccessibilitySnapshotEntry>;

// Builds a platform-neutral snapshot from a rendered Node tree. Controls
// contribute their current label/value/state; the optional focused node is
// projected as AccessibilityProperties::focused. This is deliberately a
// read-only projection: it does not claim that a platform accessibility API
// (such as Windows UI Automation) has registered a provider.
[[nodiscard]] AccessibilitySnapshot snapshotAccessibilityTree(const Node& root,
                                                               const Node* focused = nullptr);

namespace detail {

// Snapshot paths normally mirror child indices in the rendered Node tree.
// Virtualized controls reserve this impossible child index to expose logical
// children (for example, visible ListBox rows) without retaining a Node per
// option.
inline constexpr std::size_t kVirtualAccessibilityChild =
    std::numeric_limits<std::size_t>::max();

inline void appendAccessibilitySnapshot(const AccessibilityNode& node,
                                        std::vector<std::size_t>& path,
                                        AccessibilitySnapshot& snapshot)
{
    snapshot.push_back({path, path.size(), node.properties()});
    const auto& children = node.children();
    for (std::size_t index = 0; index < children.size(); ++index) {
        path.push_back(index);
        appendAccessibilitySnapshot(children[index], path, snapshot);
        path.pop_back();
    }
}

} // namespace detail

// Pre-order, stable traversal suitable for platform adapters and golden-style
// semantic tests.  The returned properties are value copies, so callers can
// inspect a frame snapshot without retaining the UI tree.
[[nodiscard]] inline AccessibilitySnapshot snapshotAccessibilityTree(const AccessibilityNode& root)
{
    AccessibilitySnapshot snapshot;
    std::vector<std::size_t> path;
    detail::appendAccessibilitySnapshot(root, path, snapshot);
    return snapshot;
}

[[nodiscard]] inline const AccessibilitySnapshotEntry* findAccessibilitySnapshotEntry(
    const AccessibilitySnapshot& snapshot, const std::vector<std::size_t>& path) noexcept
{
    for (const auto& entry : snapshot) {
        if (entry.path == path) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace wui
