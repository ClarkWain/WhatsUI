#include "wui/date_time.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <memory>
#include <utility>

#include "wui/runtime.h"
#include "wui/icons.h"
#include "wui/selection.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {
namespace {
constexpr int kEnter = 13, kSpace = 32, kEsc = 27, kLeft = 37, kUp = 38,
              kRight = 39, kDown = 40, kHome = 36, kEnd = 35, kPageUp = 33,
              kPageDown = 34;
// Fluent compat CalendarNode uses a compact 12-DIP panel inset. WhatsUI keeps the
// newer 32-DIP row rhythm requested by the native design system while the
// visible date affordance remains the Fluent 24-DIP button.
constexpr float kHeader = 32.0f, kWeek = 24.0f, kCell = 32.0f, kPad = 12.0f;
constexpr float kCalendarWidth = 7.0f * kCell + 2.0f * kPad;
constexpr float kCalendarHeight =
    kHeader + kWeek + 6.0f * kCell + 2.0f * kPad;
constexpr float kDateSurface = 24.0f;
constexpr float kHeaderButton = 28.0f;
bool leap(int y) noexcept {
  return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}
int daysInMonth(int y, int m) noexcept {
  static constexpr int days[]{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 2 ? days[m - 1] + (leap(y) ? 1 : 0) : days[m - 1];
}
int compare(CivilDate a, CivilDate b) noexcept {
  if (a.year != b.year)
    return a.year < b.year ? -1 : 1;
  if (a.month != b.month)
    return a.month < b.month ? -1 : 1;
  return a.day == b.day ? 0 : a.day < b.day ? -1 : 1;
}
CivilDate normal(CivilDate d) noexcept {
  d.month = std::clamp(d.month, 1, 12);
  d.day = std::clamp(d.day, 1, daysInMonth(d.year, d.month));
  return d;
}
std::string monthName(int month) {
  static constexpr const char *n[]{
      "January", "February", "March",     "April",   "May",      "June",
      "July",    "August",   "September", "October", "November", "December"};
  return n[std::clamp(month, 1, 12) - 1];
}
bool primary(const PointerEvent &e) noexcept {
  return e.button == MouseButton::Left;
}
bool focused(const ControlNode &n) noexcept {
  return (n.visualStates() & toMask(ControlVisualState::Focused)) != 0;
}
float textWidth(const std::string &value, const TextStyleToken &style) noexcept {
  if (const auto *measurer = textMeasurer())
    return measurer->measureText(value, style.size, style.weight).width;
  return static_cast<float>(value.size()) * style.size * 0.55f;
}
void focusRing(PaintContext &c, RectF r) {
  if (!r.width || !r.height)
    return;
  const auto &t = theme();
  const float thin = c.snapStrokeWidth(t.stroke.thin);
  r = c.snapRectEdges(r);
  c.strokeRoundRect({r.x - 2, r.y - 2, r.width + 4, r.height + 4},
                    t.radius.medium + 2, thin,
                    t.colors.strokeFocusOuter);
  c.strokeRoundRect({r.x - 1, r.y - 1, r.width + 2, r.height + 2},
                    t.radius.medium + 1, thin,
                    t.colors.strokeFocusInner);
}
void chevron(PaintContext &c, float x, float y, bool right, Color col) {
  drawIcon(c, right ? IconName::ChevronRight : IconName::ChevronLeft,
           {x - 8, y - 8, 16, 16}, col, IconSize::Size16);
}
void disclosureChevron(PaintContext &c, float x, float y, bool up, Color col) {
  drawIcon(c, up ? IconName::ChevronUp : IconName::ChevronDown,
           {x - 8, y - 8, 16, 16}, col, IconSize::Size16);
}
bool inRange(CivilDate x, std::optional<CivilDate> a,
             std::optional<CivilDate> b) {
  return a && b && compare(*a, x) <= 0 && compare(x, *b) <= 0;
}
CivilDate localToday() noexcept {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  if (localtime_s(&local, &now) != 0)
    return {};
#else
  if (localtime_r(&now, &local) == nullptr)
    return {};
#endif
  return {local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
}
bool hasState(const ControlNode &node, ControlVisualState state) noexcept {
  return (node.visualStates() & toMask(state)) != 0;
}
Color pickerSurface(const ControlNode &node, const Theme &t, bool valid) {
  if (!node.isEnabled())
    return t.colors.neutralBackgroundDisabled;
  if (hasState(node, ControlVisualState::Pressed))
    return t.colors.neutralBackground1.pressed;
  if (hasState(node, ControlVisualState::Hovered))
    return t.colors.neutralBackground1.hover;
  return t.colors.neutralBackground1.rest;
}
void paintPickerChrome(PaintContext &c, const ControlNode &node, RectF bounds,
                       bool valid) {
  const auto &t = theme();
  bounds = c.snapRectEdges(bounds);
  const bool enabled = node.isEnabled();
  const bool isFocused = focused(node) && enabled;
  Color stroke = !enabled ? t.colors.neutralStrokeDisabled
                 : !valid  ? t.colors.statusDanger
                 : hasState(node, ControlVisualState::Pressed)
                     ? t.colors.neutralStroke1Pressed
                 : hasState(node, ControlVisualState::Hovered)
                     ? t.colors.neutralStroke1Hover
                     : t.colors.neutralStroke1;
  c.fillStrokeRoundRect(bounds, t.radius.medium,
                        c.snapStrokeWidth(t.stroke.thin),
                        pickerSurface(node, t, valid), stroke);
  const Color underline =
      !enabled ? t.colors.neutralStrokeDisabled
      : isFocused ? t.colors.compoundBrandStroke.rest
      : !valid ? t.colors.statusDanger
               : t.colors.neutralStrokeAccessible;
  const float underlineWidth =
      c.snapStrokeWidth(isFocused ? t.stroke.thick : t.stroke.thin);
  c.fillRect({bounds.x + t.radius.medium,
              c.snapToPhysicalPixel(bounds.y + bounds.height -
                                    underlineWidth),
              std::max(0.0f, bounds.width - 2.0f * t.radius.medium),
              underlineWidth},
             underline);
}
class CalendarPopupNode final : public PopupNode {
public:
  CalendarPopupNode(CalendarNode *owner, std::function<void(CivilDate)> commit)
      : owner_(owner), commit_(std::move(commit)) {}
  CalendarNode *owner_;
  std::function<void(CivilDate)> commit_;
};
} // namespace
bool operator==(CivilDate a, CivilDate b) noexcept {
  return a.year == b.year && a.month == b.month && a.day == b.day;
}
bool operator!=(CivilDate a, CivilDate b) noexcept { return !(a == b); }
bool operator<(CivilDate a, CivilDate b) noexcept { return compare(a, b) < 0; }
bool isValidDate(CivilDate v) noexcept {
  return v.year >= 1 && v.month >= 1 && v.month <= 12 && v.day >= 1 &&
         v.day <= daysInMonth(v.year, v.month);
}
CivilDate addDays(CivilDate v, int days) noexcept {
  if (!isValidDate(v))
    v = normal(v);
  while (days > 0) {
    if (++v.day > daysInMonth(v.year, v.month)) {
      v.day = 1;
      if (++v.month > 12) {
        v.month = 1;
        ++v.year;
      }
    }
    --days;
  }
  while (days < 0) {
    if (--v.day < 1) {
      if (--v.month < 1) {
        v.month = 12;
        --v.year;
      }
      v.day = daysInMonth(v.year, v.month);
    }
    ++days;
  }
  return v;
}
CivilDate addMonths(CivilDate v, int n) noexcept {
  int p = v.year * 12 + v.month - 1 + n;
  v.year = p / 12;
  v.month = p % 12 + 1;
  if (v.month < 1) {
    v.month += 12;
    --v.year;
  }
  v.day = std::min(v.day, daysInMonth(v.year, v.month));
  return v;
}
int weekday(CivilDate v) noexcept {
  int y = v.year, m = v.month, d = v.day;
  if (m < 3) {
    m += 12;
    --y;
  }
  return (d + (13 * (m + 1)) / 5 + y + y / 4 - y / 100 + y / 400 + 6) % 7;
}
std::optional<CivilDate> parseIsoDate(std::string_view s) noexcept {
  int y, m, d;
  char a, b;
  if (s.size() != 10 ||
      std::sscanf(std::string(s).c_str(), "%d%c%d%c%d", &y, &a, &m, &b, &d) !=
          5 ||
      a != '-' || b != '-')
    return {};
  CivilDate out{y, m, d};
  return isValidDate(out) ? std::optional<CivilDate>(out) : std::nullopt;
}
std::string formatIsoDate(CivilDate d) {
  char b[16];
  std::snprintf(b, sizeof b, "%04d-%02d-%02d", d.year, d.month, d.day);
  return b;
}
bool operator==(CivilTime a, CivilTime b) noexcept {
  return a.hour == b.hour && a.minute == b.minute && a.second == b.second;
}
bool isValidTime(CivilTime t) noexcept {
  return t.hour >= 0 && t.hour < 24 && t.minute >= 0 && t.minute < 60 &&
         t.second >= 0 && t.second < 60;
}
std::optional<CivilTime> parseTime(std::string_view s) noexcept {
  if (s.size() != 5 && s.size() != 8)
    return {};
  if (s[2] != ':' || (s.size() == 8 && s[5] != ':'))
    return {};
  for (std::size_t i = 0; i < s.size(); ++i)
    if (i != 2 && i != 5 && (s[i] < '0' || s[i] > '9'))
      return {};
  int h = (s[0] - '0') * 10 + s[1] - '0', m = (s[3] - '0') * 10 + s[4] - '0',
      sec = s.size() == 8 ? (s[6] - '0') * 10 + s[7] - '0' : 0;
  CivilTime t{h, m, sec};
  return isValidTime(t) ? std::optional<CivilTime>(t) : std::nullopt;
}
std::string formatTime(CivilTime t, bool seconds) {
  char b[16];
  std::snprintf(b, sizeof b, seconds ? "%02d:%02d:%02d" : "%02d:%02d", t.hour,
                t.minute, t.second);
  return b;
}

CalendarNode::CalendarNode() {
  displayed_ = {2026, 1, 1};
  focused_ = displayed_;
}
CalendarNode &CalendarNode::displayedMonth(CivilDate v) {
  setDisplayedMonth(v);
  return *this;
}
void CalendarNode::setDisplayedMonth(CivilDate v) {
  v.day = 1;
  if (!isValidDate(v))
    return;
  displayed_ = v;
  markDirty(DirtyFlag::Paint);
}
CivilDate CalendarNode::displayedMonth() const noexcept { return displayed_; }
CalendarNode &CalendarNode::selectedDate(std::optional<CivilDate> v) {
  setSelectedDate(v);
  return *this;
}
void CalendarNode::setSelectedDate(std::optional<CivilDate> v) {
  if (v && !isValidDate(*v))
    v.reset();
  selected_ = v;
  mode_ = CalendarSelectionMode::Single;
  if (v) {
    focused_ = *v;
    displayed_ = {v->year, v->month, 1};
  }
  markDirty(DirtyFlag::Paint);
}
std::optional<CivilDate> CalendarNode::selectedDate() const noexcept {
  return selected_;
}
CalendarNode &CalendarNode::selectedRange(std::optional<CivilDate> a,
                                  std::optional<CivilDate> b) {
  setSelectedRange(a, b);
  return *this;
}
void CalendarNode::setSelectedRange(std::optional<CivilDate> a,
                                std::optional<CivilDate> b) {
  if (a && !isValidDate(*a))
    a.reset();
  if (b && !isValidDate(*b))
    b.reset();
  if (a && b && *b < *a)
    std::swap(a, b);
  rangeStart_ = a;
  rangeEnd_ = b;
  mode_ = CalendarSelectionMode::Range;
  if (a) {
    focused_ = *a;
    displayed_ = {a->year, a->month, 1};
  }
  markDirty(DirtyFlag::Paint);
}
std::optional<CivilDate> CalendarNode::rangeStart() const noexcept {
  return rangeStart_;
}
std::optional<CivilDate> CalendarNode::rangeEnd() const noexcept {
  return rangeEnd_;
}
CalendarNode &CalendarNode::selectionMode(CalendarSelectionMode v) {
  setSelectionMode(v);
  return *this;
}
void CalendarNode::setSelectionMode(CalendarSelectionMode v) {
  mode_ = v;
  markDirty(DirtyFlag::Paint);
}
CalendarNode &CalendarNode::minimumDate(std::optional<CivilDate> v) {
  minimum_ = v;
  return *this;
}
CalendarNode &CalendarNode::maximumDate(std::optional<CivilDate> v) {
  maximum_ = v;
  return *this;
}
CalendarNode &CalendarNode::isDateDisabled(DisablePredicate p) {
  disabled_ = std::move(p);
  return *this;
}
CalendarNode &CalendarNode::onSelect(SelectHandler h) {
  onSelect_ = std::move(h);
  return *this;
}
CivilDate CalendarNode::focusedDate() const noexcept { return focused_; }
bool CalendarNode::isDateEnabled(CivilDate v) const {
  return isValidDate(v) && (!minimum_ || compare(v, *minimum_) >= 0) &&
         (!maximum_ || compare(v, *maximum_) <= 0) &&
         (!disabled_ || !disabled_(v));
}
SizeF CalendarNode::measure(const Constraints &c) const {
  return c.clamp({kCalendarWidth, kCalendarHeight});
}
void CalendarNode::layout(const RectF &r) {
  Node::layout(r);
  clearLayoutDirtyRecursively();
}
RectF CalendarNode::dayBounds(CivilDate d) const noexcept {
  CivilDate first{displayed_.year, displayed_.month, 1};
  int pos = weekday(first) + d.day - 1;
  return {bounds().x + kPad + (pos % 7) * kCell,
          bounds().y + kHeader + kWeek + kPad + (pos / 7) * kCell, kCell,
          kCell};
}
std::optional<CivilDate> CalendarNode::dateAt(PointF p) const noexcept {
  for (int d = 1; d <= daysInMonth(displayed_.year, displayed_.month); ++d) {
    CivilDate v{displayed_.year, displayed_.month, d};
    if (dayBounds(v).contains(p))
      return v;
  }
  return {};
}
void CalendarNode::paint(PaintContext &c) {
  const auto &t = theme();
  const auto b = c.snapRectEdges(bounds());
  c.fillRoundRect(b, t.radius.large, t.colors.neutralBackground1.rest);
  std::string title =
      monthName(displayed_.month) + " " + std::to_string(displayed_.year);
  c.drawText(title, b.x + kPad,
             c.centeredTextBottom(
                 title, {b.x + kPad, b.y, b.width - 2 * kPad, kHeader},
                 t.typography.body1Strong.size,
                 t.typography.body1Strong.weight),
             t.typography.body1Strong.size, t.colors.neutralForeground1,
             t.typography.body1Strong.weight,
             t.typography.body1Strong.family);
  const RectF previous{b.x + b.width - kPad - 2.0f * kHeaderButton, b.y + 2,
                       kHeaderButton, kHeaderButton};
  const RectF next{previous.x + kHeaderButton, previous.y, kHeaderButton,
                   kHeaderButton};
  if (hoveredHeader_ == -1 || pressedHeader_ == -1)
    c.fillRoundRect(previous, t.radius.medium,
                    pressedHeader_ == -1 ? t.colors.neutralBackground1.pressed
                                         : t.colors.neutralBackground1.hover);
  if (hoveredHeader_ == 1 || pressedHeader_ == 1)
    c.fillRoundRect(next, t.radius.medium,
                    pressedHeader_ == 1 ? t.colors.neutralBackground1.pressed
                                        : t.colors.neutralBackground1.hover);
  chevron(c, previous.x + previous.width * .5f,
          previous.y + previous.height * .5f, false,
          t.colors.neutralForeground2);
  chevron(c, next.x + next.width * .5f, next.y + next.height * .5f, true,
          t.colors.neutralForeground2);
  static constexpr const char *labels[]{"S", "M", "T", "W", "T", "F", "S"};
  for (int i = 0; i < 7; ++i) {
    const auto &style = t.typography.caption1;
    const std::string label{labels[i]};
    const RectF cell{b.x + kPad + i * kCell, b.y + kHeader, kCell, kWeek};
    c.drawText(label, cell.x + (cell.width - textWidth(label, style)) * 0.5f,
               c.centeredTextBottom(label, cell, style.size, style.weight,
                                    style.family),
               style.size, t.colors.neutralForeground2, style.weight,
               style.family);
  }
  for (int d = 1; d <= daysInMonth(displayed_.year, displayed_.month); ++d) {
    CivilDate v{displayed_.year, displayed_.month, d};
    auto r = dayBounds(v);
    const bool insideRange = mode_ == CalendarSelectionMode::Range &&
                             inRange(v, rangeStart_, rangeEnd_);
    const bool rangeEndpoint = insideRange &&
                               ((rangeStart_ && *rangeStart_ == v) ||
                                (rangeEnd_ && *rangeEnd_ == v));
    const bool selectedCircle = mode_ == CalendarSelectionMode::Single
                                    ? selected_ && *selected_ == v
                                    : rangeEndpoint;
    const bool isToday = v == localToday();
    const RectF surface{r.x + (r.width - kDateSurface) * .5f,
                        r.y + (r.height - kDateSurface) * .5f,
                        kDateSurface, kDateSurface};
    // Range selection is a continuous 24-DIP band, matching the visible day
    // button rather than flooding the full 32-DIP hit row.
    if (insideRange)
      c.fillRect({r.x, surface.y, r.width, surface.height},
                 t.colors.neutralBackground3.selected);
    if (hoveredDate_ && *hoveredDate_ == v && isDateEnabled(v) &&
        !selectedCircle)
      c.fillRoundRect(surface, t.radius.medium,
                      t.colors.neutralBackground1.hover);
    if (pressedDate_ && *pressedDate_ == v && isDateEnabled(v) &&
        !selectedCircle)
      c.fillRoundRect(surface, t.radius.medium,
                      t.colors.neutralBackground1.pressed);
    if (selectedCircle)
      c.fillStrokeRoundRect(surface, t.radius.medium,
                            c.snapStrokeWidth(t.stroke.thin),
                            t.colors.brandBackground.rest,
                            t.colors.compoundBrandStroke.rest);
    else if (isToday) {
      const RectF marker{r.x + (r.width - 20.0f) * .5f,
                         r.y + (r.height - 20.0f) * .5f, 20.0f, 20.0f};
      c.fillRoundRect(marker, t.radius.circular,
                      t.colors.brandBackground.rest);
    }
    if (v == focused_ && focused(*this))
      focusRing(c, surface);
    const auto col = !isDateEnabled(v) ? t.colors.neutralForegroundDisabled
                     : selectedCircle || isToday ? t.colors.onBrand
                                       : t.colors.neutralForeground1;
    auto text = std::to_string(d);
    const auto &style =
        isToday ? t.typography.body1Strong : t.typography.body1;
    c.drawText(text, r.x + (r.width - textWidth(text, style)) * 0.5f,
               c.centeredTextBottom(text, r, t.typography.body1.size,
                                    t.typography.body1.weight,
                                    t.typography.body1.family),
               t.typography.body1.size, col, t.typography.body1.weight,
               t.typography.body1.family);
  }
  clearDirty(DirtyFlag::Paint);
}
void CalendarNode::setFocused(CivilDate v) {
  focused_ = v;
  displayed_ = {v.year, v.month, 1};
  markDirty(DirtyFlag::Paint);
}
void CalendarNode::moveFocus(int n) {
  CivilDate v = addDays(focused_, n);
  setFocused(v);
}
void CalendarNode::select(CivilDate v) {
  if (!isDateEnabled(v))
    return;
  if (mode_ == CalendarSelectionMode::Single)
    selected_ = v;
  else if (!rangeStart_ || rangeEnd_) {
    rangeStart_ = v;
    rangeEnd_.reset();
  } else {
    rangeEnd_ = v;
    if (*rangeEnd_ < *rangeStart_)
      std::swap(rangeStart_, rangeEnd_);
  }
  if (onSelect_)
    onSelect_(mode_ == CalendarSelectionMode::Single ? selected_ : rangeStart_,
              mode_ == CalendarSelectionMode::Single ? std::nullopt
                                                     : rangeEnd_);
  markDirty(DirtyFlag::Paint);
}
bool CalendarNode::onPointerEvent(const PointerEvent &e) {
  if (!isEnabled())
    return false;
  if (e.action == PointerAction::Leave ||
      (e.action == PointerAction::Move && !bounds().contains(e.position))) {
    hoveredDate_.reset();
    pressedDate_.reset();
    hoveredHeader_ = 0;
    pressedHeader_ = 0;
    markDirty(DirtyFlag::Paint);
    return true;
  }
  if (!bounds().contains(e.position))
    return false;
  if (e.action == PointerAction::Enter || e.action == PointerAction::Move) {
    const auto b = bounds();
    const RectF previous{b.x + b.width - kPad - 2.0f * kHeaderButton,
                         b.y + 2, kHeaderButton, kHeaderButton};
    const RectF next{previous.x + kHeaderButton, previous.y, kHeaderButton,
                     kHeaderButton};
    hoveredHeader_ = previous.contains(e.position)
                         ? -1
                         : next.contains(e.position) ? 1 : 0;
    hoveredDate_ = hoveredHeader_ == 0 ? dateAt(e.position)
                                       : std::optional<CivilDate>{};
    markDirty(DirtyFlag::Paint);
    return true;
  }
  if (e.action == PointerAction::Down && primary(e)) {
    setVisualState(ControlVisualState::Focused, true);
    const auto b = bounds();
    const RectF previous{b.x + b.width - kPad - 2.0f * kHeaderButton,
                         b.y + 2, kHeaderButton, kHeaderButton};
    const RectF next{previous.x + kHeaderButton, previous.y, kHeaderButton,
                     kHeaderButton};
    pressedHeader_ = previous.contains(e.position)
                         ? -1
                         : next.contains(e.position) ? 1 : 0;
    pressedDate_ = pressedHeader_ == 0 ? dateAt(e.position)
                                       : std::optional<CivilDate>{};
    markDirty(DirtyFlag::Paint);
    return true;
  }
  if (e.action == PointerAction::Up && primary(e)) {
    const auto b = bounds();
    const RectF previous{b.x + b.width - kPad - 2.0f * kHeaderButton,
                         b.y + 2, kHeaderButton, kHeaderButton};
    const RectF next{previous.x + kHeaderButton, previous.y, kHeaderButton,
                     kHeaderButton};
    const int releasedHeader = previous.contains(e.position)
                                   ? -1
                                   : next.contains(e.position) ? 1 : 0;
    if (releasedHeader != 0 && releasedHeader == pressedHeader_) {
      displayed_ = addMonths(displayed_, releasedHeader);
      focused_ = addMonths(focused_, releasedHeader);
    }
    const auto released = dateAt(e.position);
    if (releasedHeader == 0 && released && pressedDate_ &&
        *released == *pressedDate_)
      select(*released);
    pressedDate_.reset();
    pressedHeader_ = 0;
    hoveredDate_ = released;
    markDirty(DirtyFlag::Paint);
    return true;
  }
  if (e.action == PointerAction::Cancel) {
    pressedDate_.reset();
    pressedHeader_ = 0;
    markDirty(DirtyFlag::Paint);
    return true;
  }
  return false;
}
bool CalendarNode::onKeyEvent(const KeyEvent &e) {
  if (e.action != KeyAction::Down || !isEnabled())
    return false;
  switch (e.keyCode) {
  case kLeft:
    moveFocus(-1);
    break;
  case kRight:
    moveFocus(1);
    break;
  case kUp:
    moveFocus(-7);
    break;
  case kDown:
    moveFocus(7);
    break;
  case kPageUp:
    setFocused(addMonths(focused_, -1));
    break;
  case kPageDown:
    setFocused(addMonths(focused_, 1));
    break;
  case kHome:
    moveFocus(-weekday(focused_));
    break;
  case kEnd:
    moveFocus(6 - weekday(focused_));
    break;
  case kEnter:
  case kSpace:
    select(focused_);
    break;
  default:
    return false;
  }
  return true;
}

DatePickerNode::DatePickerNode(std::string p) : placeholder_(std::move(p)) {}
DatePickerNode::~DatePickerNode() { closePopup(); }
DatePickerNode &DatePickerNode::value(std::optional<CivilDate> v) {
  setValue(v);
  return *this;
}
void DatePickerNode::setValue(std::optional<CivilDate> v) {
  if (v && !isValidDate(*v))
    v.reset();
  value_ = v;
  text_ = v ? formatIsoDate(*v) : "";
  valid_ = true;
  markDirty(DirtyFlag::Paint);
}
std::optional<CivilDate> DatePickerNode::value() const noexcept { return value_; }
DatePickerNode &DatePickerNode::text(std::string v) {
  text_ = std::move(v);
  validateText();
  return *this;
}
const std::string &DatePickerNode::text() const noexcept { return text_; }
bool DatePickerNode::isValid() const noexcept { return valid_; }
DatePickerNode &DatePickerNode::placeholder(std::string v) {
  placeholder_ = std::move(v);
  return *this;
}
DatePickerNode &DatePickerNode::minimumDate(std::optional<CivilDate> v) {
  minimum_ = v;
  return *this;
}
DatePickerNode &DatePickerNode::maximumDate(std::optional<CivilDate> v) {
  maximum_ = v;
  return *this;
}
DatePickerNode &DatePickerNode::bindOverlayHost(OverlayHost &h) noexcept {
  host_ = &h;
  return *this;
}
DatePickerNode &DatePickerNode::onChange(ChangeHandler h) {
  onChange_ = std::move(h);
  return *this;
}
bool DatePickerNode::isOpen() const noexcept { return open_; }
SizeF DatePickerNode::measure(const Constraints &c) const {
  return c.clamp({180, theme().controls.height});
}
void DatePickerNode::paint(PaintContext &c) {
  const auto &t = theme();
  auto b = c.snapRectEdges(bounds());
  paintPickerChrome(c, *this, b, valid_);
  auto &s = text_.empty() ? placeholder_ : text_;
  c.drawText(s, b.x + 12,
             c.centeredTextBottom(s, b, t.typography.body1.size,
                                  t.typography.body1.weight),
             t.typography.body1.size,
             !isEnabled() ? t.colors.neutralForegroundDisabled
             : text_.empty() ? t.colors.neutralForeground3
                             : t.colors.neutralForeground1,
             t.typography.body1.weight, t.typography.body1.family);
  disclosureChevron(c, b.x + b.width - 16, b.y + b.height / 2, open_,
                    isEnabled() ? t.colors.neutralForeground2
                                : t.colors.neutralForegroundDisabled);
  clearDirty(DirtyFlag::Paint);
}
void DatePickerNode::validateText() {
  if (text_.empty()) {
    value_.reset();
    valid_ = true;
  } else {
    auto p = parseIsoDate(text_);
    valid_ = p && (!minimum_ || compare(*p, *minimum_) >= 0) &&
             (!maximum_ || compare(*p, *maximum_) <= 0);
    if (valid_)
      value_ = p;
  }
  markDirty(DirtyFlag::Paint);
}
void DatePickerNode::commit(std::optional<CivilDate> v) {
  setValue(v);
  if (onChange_)
    onChange_(value_);
  closePopup();
}
void DatePickerNode::openPopup() {
  if (open_ || !host_)
    return;
  auto calendar = std::make_unique<CalendarNode>();
  calendar->setSelectedDate(value_);
  calendar->minimumDate(minimum_).maximumDate(maximum_);
  calendar->onSelect([this](auto first, auto) { commit(first); });
  CalendarNode *raw = calendar.get();
  auto popup = std::make_unique<PopupNode>();
  popup->anchor(bounds()).preferredSize({kCalendarWidth, 0}).onDismiss([this] {
    closePopup();
  });
  popup->content(std::move(calendar));
  overlay_ = host_->show(std::move(popup));
  open_ = true;
  host_->focus(raw);
  markDirty(DirtyFlag::Paint);
}
void DatePickerNode::closePopup() {
  OverlayHost *host = host_;
  const auto overlay = overlay_;
  open_ = false;
  overlay_ = 0;
  if (host && overlay) {
    (void)host->dismiss(overlay);
    host->focus(this);
  }
  setVisualState(ControlVisualState::Focused, true);
  markDirty(DirtyFlag::Paint);
}
bool DatePickerNode::onPointerEvent(const PointerEvent &e) {
  if (!isEnabled())
    return false;
  if (e.action == PointerAction::Enter ||
      (e.action == PointerAction::Move && bounds().contains(e.position))) {
    setVisualState(ControlVisualState::Hovered, true);
    return true;
  }
  if (e.action == PointerAction::Leave) {
    setVisualState(ControlVisualState::Hovered, false);
    setVisualState(ControlVisualState::Pressed, false);
    return true;
  }
  if (e.action == PointerAction::Down && primary(e) &&
      bounds().contains(e.position)) {
    setVisualState(ControlVisualState::Focused, true);
    setVisualState(ControlVisualState::Pressed, true);
    open_ ? closePopup() : openPopup();
    return true;
  }
  if (e.action == PointerAction::Cancel ||
      (e.action == PointerAction::Up && primary(e))) {
    setVisualState(ControlVisualState::Pressed, false);
    return true;
  }
  return false;
}
bool DatePickerNode::onKeyEvent(const KeyEvent &e) {
  if (e.action != KeyAction::Down)
    return false;
  if (e.keyCode == kEsc) {
    closePopup();
    return true;
  }
  if (e.keyCode == 8 && !text_.empty()) {
    text_.pop_back();
    validateText();
    return true;
  }
  if (e.keyCode == kEnter || e.keyCode == kSpace || e.keyCode == kDown) {
    openPopup();
    return true;
  }
  return false;
}
bool DatePickerNode::onTextInput(const TextInputEvent &e) {
  if (e.text.empty())
    return false;
  text_ += e.text;
  validateText();
  return true;
}
AccessibilityActionCapabilities DatePickerNode::accessibilityActions() const noexcept {
  AccessibilityActionCapabilities actions;
  actions.expandCollapse = host_ != nullptr;
  actions.setValue = true;
  return actions;
}
AccessibilityActionStatus DatePickerNode::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value) {
  if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
  if (kind == AccessibilityActionKind::Expand) {
    openPopup();
    return open_ ? AccessibilityActionStatus::Succeeded : AccessibilityActionStatus::Failed;
  }
  if (kind == AccessibilityActionKind::Collapse) {
    closePopup();
    return AccessibilityActionStatus::Succeeded;
  }
  if (kind == AccessibilityActionKind::SetValue) {
    if (value.empty()) { commit({}); return AccessibilityActionStatus::Succeeded; }
    const auto parsed = parseIsoDate(value);
    if (!parsed || (minimum_ && compare(*parsed, *minimum_) < 0) ||
        (maximum_ && compare(*parsed, *maximum_) > 0)) return AccessibilityActionStatus::InvalidValue;
    commit(parsed);
    return AccessibilityActionStatus::Succeeded;
  }
  return AccessibilityActionStatus::NotSupported;
}

TimePickerNode::TimePickerNode(std::string p) : placeholder_(std::move(p)) {}
TimePickerNode::~TimePickerNode() { closePopup(); }
TimePickerNode &TimePickerNode::value(std::optional<CivilTime> v) {
  setValue(v);
  return *this;
}
void TimePickerNode::setValue(std::optional<CivilTime> v) {
  if (v && !isValidTime(*v))
    v.reset();
  value_ = v;
  text_ = v ? formatTime(*v) : "";
  valid_ = true;
  markDirty(DirtyFlag::Paint);
}
std::optional<CivilTime> TimePickerNode::value() const noexcept { return value_; }
TimePickerNode &TimePickerNode::text(std::string v) {
  text_ = std::move(v);
  validateText();
  return *this;
}
const std::string &TimePickerNode::text() const noexcept { return text_; }
bool TimePickerNode::isValid() const noexcept { return valid_; }
TimePickerNode &TimePickerNode::minuteStep(int v) {
  minuteStep_ = std::clamp(v, 1, 60);
  return *this;
}
int TimePickerNode::minuteStep() const noexcept { return minuteStep_; }
TimePickerNode &TimePickerNode::bindOverlayHost(OverlayHost &h) noexcept { host_ = &h; return *this; }
TimePickerNode &TimePickerNode::onChange(ChangeHandler h) {
  onChange_ = std::move(h);
  return *this;
}
bool TimePickerNode::isOpen() const noexcept { return open_; }
SizeF TimePickerNode::measure(const Constraints &c) const {
  return c.clamp({150, theme().controls.height});
}
void TimePickerNode::paint(PaintContext &c) {
  const auto &t = theme();
  auto b = c.snapRectEdges(bounds());
  paintPickerChrome(c, *this, b, valid_);
  auto &s = text_.empty() ? placeholder_ : text_;
  c.drawText(s, b.x + 12,
             c.centeredTextBottom(s, b, t.typography.body1.size,
                                  t.typography.body1.weight),
             t.typography.body1.size,
             !isEnabled() ? t.colors.neutralForegroundDisabled
             : text_.empty() ? t.colors.neutralForeground3
                             : t.colors.neutralForeground1,
             t.typography.body1.weight, t.typography.body1.family);
  disclosureChevron(c, b.x + b.width - 16, b.y + b.height / 2, open_,
                    isEnabled() ? t.colors.neutralForeground2
                                : t.colors.neutralForegroundDisabled);
  clearDirty(DirtyFlag::Paint);
}
void TimePickerNode::validateText() {
  if (text_.empty()) {
    value_.reset();
    valid_ = true;
  } else {
    auto p = parseTime(text_);
    valid_ = p && p->minute % minuteStep_ == 0;
    if (valid_)
      value_ = p;
  }
  markDirty(DirtyFlag::Paint);
}
void TimePickerNode::commit(std::optional<CivilTime> v) {
  setValue(v);
  if (onChange_)
    onChange_(value_);
}
void TimePickerNode::openPopup() {
  if (open_ || !host_) return;
  auto list = std::make_unique<ListBoxNode>();
  ListBoxNode *raw = list.get();
  const int count = 24 * 60 / minuteStep_;
  for (int index = 0; index < count; ++index) {
    const CivilTime time{index * minuteStep_ / 60, index * minuteStep_ % 60, 0};
    const auto formatted = formatTime(time);
    list->addOption({formatted, formatted});
  }
  list->setAccessibleLabel("Time options");
  // Keep the disclosure compact enough to remain below a field in a short
  // desktop window. The list remains scrollable and opens at the committed
  // value, so five rows preserve context without covering its trigger.
  list->setMaxVisibleOptions(5);
  if (value_) list->setSelectedIndex((value_->hour * 60 + value_->minute) / minuteStep_);
  list->onSelectionChanged([this](int, const Option &option) {
    const auto picked = parseTime(option.value);
    if (picked) commit(picked);
    closePopup();
  });
  auto popup = std::make_unique<PopupNode>();
  popup->anchor(bounds()).preferredSize({std::max(180.0f, bounds().width), 0.0f})
      .onDismiss([this] { closePopup(); });
  popup->content(std::move(list));
  overlay_ = host_->show(std::move(popup));
  open_ = true;
  host_->focus(raw);
  markDirty(DirtyFlag::Paint);
}
void TimePickerNode::closePopup() {
  OverlayHost *host = host_;
  const auto overlay = overlay_;
  open_ = false;
  overlay_ = 0;
  if (host && overlay) {
    (void)host->dismiss(overlay);
    host->focus(this);
  }
  markDirty(DirtyFlag::Paint);
}
bool TimePickerNode::onPointerEvent(const PointerEvent &e) {
  if (!isEnabled())
    return false;
  if (e.action == PointerAction::Enter ||
      (e.action == PointerAction::Move && bounds().contains(e.position))) {
    setVisualState(ControlVisualState::Hovered, true);
    return true;
  }
  if (e.action == PointerAction::Leave) {
    setVisualState(ControlVisualState::Hovered, false);
    setVisualState(ControlVisualState::Pressed, false);
    return true;
  }
  if (e.action == PointerAction::Down && primary(e) &&
      bounds().contains(e.position)) {
    setVisualState(ControlVisualState::Focused, true);
    setVisualState(ControlVisualState::Pressed, true);
    open_ ? closePopup() : openPopup();
    return true;
  }
  if (e.action == PointerAction::Cancel ||
      (e.action == PointerAction::Up && primary(e))) {
    setVisualState(ControlVisualState::Pressed, false);
    return true;
  }
  return false;
}
bool TimePickerNode::onKeyEvent(const KeyEvent &e) {
  if (e.action != KeyAction::Down)
    return false;
  if (e.keyCode == 8 && !text_.empty()) {
    text_.pop_back();
    validateText();
    return true;
  }
  if (e.keyCode == kEsc && open_) { closePopup(); return true; }
  if (e.keyCode == kEnter || e.keyCode == kSpace || e.keyCode == kDown) {
    if (host_) { openPopup(); return true; }
  }
  if (e.keyCode == kUp || e.keyCode == kDown) {
    CivilTime t = value_.value_or(CivilTime{});
    int total = t.hour * 60 + t.minute +
                (e.keyCode == kUp ? minuteStep_ : -minuteStep_);
    total = (total % 1440 + 1440) % 1440;
    t = {total / 60, total % 60, 0};
    commit(t);
    return true;
  }
  return false;
}
bool TimePickerNode::onTextInput(const TextInputEvent &e) {
  if (e.text.empty())
    return false;
  text_ += e.text;
  validateText();
  return true;
}
AccessibilityActionCapabilities TimePickerNode::accessibilityActions() const noexcept {
  AccessibilityActionCapabilities actions;
  actions.expandCollapse = host_ != nullptr;
  actions.setValue = true;
  return actions;
}
AccessibilityActionStatus TimePickerNode::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value) {
  if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
  if (kind == AccessibilityActionKind::Expand) {
    openPopup();
    return open_ ? AccessibilityActionStatus::Succeeded : AccessibilityActionStatus::Failed;
  }
  if (kind == AccessibilityActionKind::Collapse) { closePopup(); return AccessibilityActionStatus::Succeeded; }
  if (kind == AccessibilityActionKind::SetValue) {
    if (value.empty()) { commit({}); return AccessibilityActionStatus::Succeeded; }
    const auto parsed = parseTime(value);
    if (!parsed || parsed->minute % minuteStep_ != 0) return AccessibilityActionStatus::InvalidValue;
    commit(parsed);
    return AccessibilityActionStatus::Succeeded;
  }
  return AccessibilityActionStatus::NotSupported;
}
} // namespace wui
