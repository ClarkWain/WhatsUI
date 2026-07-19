#include "wui/date_time.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
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
constexpr float kHeader = 40.0f, kWeek = 22.0f, kCell = 36.0f, kPad = 12.0f;
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
  c.strokeRoundRect({r.x - 2, r.y - 2, r.width + 4, r.height + 4},
                    t.radius.medium + 2, t.stroke.thin,
                    t.colors.strokeFocusOuter);
  c.strokeRoundRect({r.x - 1, r.y - 1, r.width + 2, r.height + 2},
                    t.radius.medium + 1, t.stroke.thin,
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
class CalendarPopup final : public Popup {
public:
  CalendarPopup(Calendar *owner, std::function<void(CivilDate)> commit)
      : owner_(owner), commit_(std::move(commit)) {}
  Calendar *owner_;
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

Calendar::Calendar() {
  displayed_ = {2026, 1, 1};
  focused_ = displayed_;
}
Calendar &Calendar::displayedMonth(CivilDate v) {
  setDisplayedMonth(v);
  return *this;
}
void Calendar::setDisplayedMonth(CivilDate v) {
  v.day = 1;
  if (!isValidDate(v))
    return;
  displayed_ = v;
  markDirty(DirtyFlag::Paint);
}
CivilDate Calendar::displayedMonth() const noexcept { return displayed_; }
Calendar &Calendar::selectedDate(std::optional<CivilDate> v) {
  setSelectedDate(v);
  return *this;
}
void Calendar::setSelectedDate(std::optional<CivilDate> v) {
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
std::optional<CivilDate> Calendar::selectedDate() const noexcept {
  return selected_;
}
Calendar &Calendar::selectedRange(std::optional<CivilDate> a,
                                  std::optional<CivilDate> b) {
  setSelectedRange(a, b);
  return *this;
}
void Calendar::setSelectedRange(std::optional<CivilDate> a,
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
std::optional<CivilDate> Calendar::rangeStart() const noexcept {
  return rangeStart_;
}
std::optional<CivilDate> Calendar::rangeEnd() const noexcept {
  return rangeEnd_;
}
Calendar &Calendar::selectionMode(CalendarSelectionMode v) {
  setSelectionMode(v);
  return *this;
}
void Calendar::setSelectionMode(CalendarSelectionMode v) {
  mode_ = v;
  markDirty(DirtyFlag::Paint);
}
Calendar &Calendar::minimumDate(std::optional<CivilDate> v) {
  minimum_ = v;
  return *this;
}
Calendar &Calendar::maximumDate(std::optional<CivilDate> v) {
  maximum_ = v;
  return *this;
}
Calendar &Calendar::isDateDisabled(DisablePredicate p) {
  disabled_ = std::move(p);
  return *this;
}
Calendar &Calendar::onSelect(SelectHandler h) {
  onSelect_ = std::move(h);
  return *this;
}
CivilDate Calendar::focusedDate() const noexcept { return focused_; }
bool Calendar::isDateEnabled(CivilDate v) const {
  return isValidDate(v) && (!minimum_ || compare(v, *minimum_) >= 0) &&
         (!maximum_ || compare(v, *maximum_) <= 0) &&
         (!disabled_ || !disabled_(v));
}
SizeF Calendar::measure(const Constraints &c) const {
  return c.clamp(
      {7 * kCell + 2 * kPad, kHeader + kWeek + 6 * kCell + 2 * kPad});
}
void Calendar::layout(const RectF &r) {
  Node::layout(r);
  clearLayoutDirtyRecursively();
}
RectF Calendar::dayBounds(CivilDate d) const noexcept {
  CivilDate first{displayed_.year, displayed_.month, 1};
  int pos = weekday(first) + d.day - 1;
  return {bounds().x + kPad + (pos % 7) * kCell,
          bounds().y + kHeader + kWeek + kPad + (pos / 7) * kCell, kCell,
          kCell};
}
std::optional<CivilDate> Calendar::dateAt(PointF p) const noexcept {
  for (int d = 1; d <= daysInMonth(displayed_.year, displayed_.month); ++d) {
    CivilDate v{displayed_.year, displayed_.month, d};
    if (dayBounds(v).contains(p))
      return v;
  }
  return {};
}
void Calendar::paint(PaintContext &c) {
  const auto &t = theme();
  const auto b = bounds();
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
  chevron(c, b.x + b.width - kPad - 30, b.y + kHeader / 2, false,
          t.colors.neutralForeground2);
  chevron(c, b.x + b.width - kPad - 10, b.y + kHeader / 2, true,
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
    // A range is a single continuous band. Fill each whole cell first so the
    // colour reaches the adjacent grid cell without visual gaps, then layer
    // the two endpoint circles on top as the explicit selection affordances.
    if (insideRange)
      c.fillRect(r, t.colors.neutralBackground3.selected);
    if (selectedCircle)
      c.fillRoundRect({r.x + 3, r.y + 3, r.width - 6, r.height - 6},
                      t.radius.circular, t.colors.brandBackground.rest);
    if (v == focused_ && focused(*this))
      focusRing(c, {r.x + 4, r.y + 4, r.width - 8, r.height - 8});
    const auto col = !isDateEnabled(v) ? t.colors.neutralForegroundDisabled
                     : selectedCircle  ? t.colors.onBrand
                                       : t.colors.neutralForeground1;
    auto text = std::to_string(d);
    const auto &style = t.typography.body1;
    c.drawText(text, r.x + (r.width - textWidth(text, style)) * 0.5f,
               c.centeredTextBottom(text, r, t.typography.body1.size,
                                    t.typography.body1.weight,
                                    t.typography.body1.family),
               t.typography.body1.size, col, t.typography.body1.weight,
               t.typography.body1.family);
  }
  clearDirty(DirtyFlag::Paint);
}
void Calendar::setFocused(CivilDate v) {
  focused_ = v;
  displayed_ = {v.year, v.month, 1};
  markDirty(DirtyFlag::Paint);
}
void Calendar::moveFocus(int n) {
  CivilDate v = addDays(focused_, n);
  setFocused(v);
}
void Calendar::select(CivilDate v) {
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
bool Calendar::onPointerEvent(const PointerEvent &e) {
  if (!bounds().contains(e.position) || !isEnabled())
    return false;
  if (e.action == PointerAction::Down && primary(e)) {
    setVisualState(ControlVisualState::Focused, true);
    if (auto d = dateAt(e.position))
      select(*d);
    return true;
  }
  return false;
}
bool Calendar::onKeyEvent(const KeyEvent &e) {
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

DatePicker::DatePicker(std::string p) : placeholder_(std::move(p)) {}
DatePicker::~DatePicker() { closePopup(); }
DatePicker &DatePicker::value(std::optional<CivilDate> v) {
  setValue(v);
  return *this;
}
void DatePicker::setValue(std::optional<CivilDate> v) {
  if (v && !isValidDate(*v))
    v.reset();
  value_ = v;
  text_ = v ? formatIsoDate(*v) : "";
  valid_ = true;
  markDirty(DirtyFlag::Paint);
}
std::optional<CivilDate> DatePicker::value() const noexcept { return value_; }
DatePicker &DatePicker::text(std::string v) {
  text_ = std::move(v);
  validateText();
  return *this;
}
const std::string &DatePicker::text() const noexcept { return text_; }
bool DatePicker::isValid() const noexcept { return valid_; }
DatePicker &DatePicker::placeholder(std::string v) {
  placeholder_ = std::move(v);
  return *this;
}
DatePicker &DatePicker::minimumDate(std::optional<CivilDate> v) {
  minimum_ = v;
  return *this;
}
DatePicker &DatePicker::maximumDate(std::optional<CivilDate> v) {
  maximum_ = v;
  return *this;
}
DatePicker &DatePicker::bindOverlayHost(OverlayHost &h) noexcept {
  host_ = &h;
  return *this;
}
DatePicker &DatePicker::onChange(ChangeHandler h) {
  onChange_ = std::move(h);
  return *this;
}
bool DatePicker::isOpen() const noexcept { return open_; }
SizeF DatePicker::measure(const Constraints &c) const {
  return c.clamp({180, theme().controls.height});
}
void DatePicker::paint(PaintContext &c) {
  const auto &t = theme();
  auto b = bounds();
  c.fillStrokeRoundRect(
      b, t.radius.medium, t.stroke.thin,
      valid_ ? t.colors.neutralBackground1.rest
             : t.colors.dangerBackground.rest,
      valid_ ? t.colors.neutralStroke1 : t.colors.statusDanger);
  auto &s = text_.empty() ? placeholder_ : text_;
  c.drawText(s, b.x + 12,
             c.centeredTextBottom(s, b, t.typography.body1.size,
                                  t.typography.body1.weight),
             t.typography.body1.size,
             text_.empty() ? t.colors.neutralForeground3
                           : t.colors.neutralForeground1,
             t.typography.body1.weight, t.typography.body1.family);
  disclosureChevron(c, b.x + b.width - 16, b.y + b.height / 2, open_,
                    t.colors.neutralForeground2);
  if (focused(*this))
    focusRing(c, b);
  clearDirty(DirtyFlag::Paint);
}
void DatePicker::validateText() {
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
void DatePicker::commit(std::optional<CivilDate> v) {
  setValue(v);
  if (onChange_)
    onChange_(value_);
  closePopup();
}
void DatePicker::openPopup() {
  if (open_ || !host_)
    return;
  auto calendar = std::make_unique<Calendar>();
  calendar->setSelectedDate(value_);
  calendar->minimumDate(minimum_).maximumDate(maximum_);
  calendar->onSelect([this](auto first, auto) { commit(first); });
  Calendar *raw = calendar.get();
  auto popup = std::make_unique<Popup>();
  popup->anchor(bounds()).preferredSize({276, 0}).onDismiss([this] {
    closePopup();
  });
  popup->content(std::move(calendar));
  overlay_ = host_->show(std::move(popup));
  open_ = true;
  host_->focus(raw);
  markDirty(DirtyFlag::Paint);
}
void DatePicker::closePopup() {
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
bool DatePicker::onPointerEvent(const PointerEvent &e) {
  if (e.action == PointerAction::Down && primary(e) &&
      bounds().contains(e.position)) {
    setVisualState(ControlVisualState::Focused, true);
    open_ ? closePopup() : openPopup();
    return true;
  }
  return false;
}
bool DatePicker::onKeyEvent(const KeyEvent &e) {
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
bool DatePicker::onTextInput(const TextInputEvent &e) {
  if (e.text.empty())
    return false;
  text_ += e.text;
  validateText();
  return true;
}
AccessibilityActionCapabilities DatePicker::accessibilityActions() const noexcept {
  AccessibilityActionCapabilities actions;
  actions.expandCollapse = host_ != nullptr;
  actions.setValue = true;
  return actions;
}
AccessibilityActionStatus DatePicker::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value) {
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

TimePicker::TimePicker(std::string p) : placeholder_(std::move(p)) {}
TimePicker::~TimePicker() { closePopup(); }
TimePicker &TimePicker::value(std::optional<CivilTime> v) {
  setValue(v);
  return *this;
}
void TimePicker::setValue(std::optional<CivilTime> v) {
  if (v && !isValidTime(*v))
    v.reset();
  value_ = v;
  text_ = v ? formatTime(*v) : "";
  valid_ = true;
  markDirty(DirtyFlag::Paint);
}
std::optional<CivilTime> TimePicker::value() const noexcept { return value_; }
TimePicker &TimePicker::text(std::string v) {
  text_ = std::move(v);
  validateText();
  return *this;
}
const std::string &TimePicker::text() const noexcept { return text_; }
bool TimePicker::isValid() const noexcept { return valid_; }
TimePicker &TimePicker::minuteStep(int v) {
  minuteStep_ = std::clamp(v, 1, 60);
  return *this;
}
int TimePicker::minuteStep() const noexcept { return minuteStep_; }
TimePicker &TimePicker::bindOverlayHost(OverlayHost &h) noexcept { host_ = &h; return *this; }
TimePicker &TimePicker::onChange(ChangeHandler h) {
  onChange_ = std::move(h);
  return *this;
}
bool TimePicker::isOpen() const noexcept { return open_; }
SizeF TimePicker::measure(const Constraints &c) const {
  return c.clamp({150, theme().controls.height});
}
void TimePicker::paint(PaintContext &c) {
  const auto &t = theme();
  auto b = bounds();
  c.fillStrokeRoundRect(
      b, t.radius.medium, t.stroke.thin,
      valid_ ? t.colors.neutralBackground1.rest
             : t.colors.dangerBackground.rest,
      valid_ ? t.colors.neutralStroke1 : t.colors.statusDanger);
  auto &s = text_.empty() ? placeholder_ : text_;
  c.drawText(s, b.x + 12,
             c.centeredTextBottom(s, b, t.typography.body1.size,
                                  t.typography.body1.weight),
             t.typography.body1.size,
             text_.empty() ? t.colors.neutralForeground3
                           : t.colors.neutralForeground1,
             t.typography.body1.weight, t.typography.body1.family);
  disclosureChevron(c, b.x + b.width - 16, b.y + b.height / 2, open_,
                    t.colors.neutralForeground2);
  if (focused(*this))
    focusRing(c, b);
  clearDirty(DirtyFlag::Paint);
}
void TimePicker::validateText() {
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
void TimePicker::commit(std::optional<CivilTime> v) {
  setValue(v);
  if (onChange_)
    onChange_(value_);
}
void TimePicker::openPopup() {
  if (open_ || !host_) return;
  auto list = std::make_unique<ListBox>();
  ListBox *raw = list.get();
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
  auto popup = std::make_unique<Popup>();
  popup->anchor(bounds()).preferredSize({std::max(180.0f, bounds().width), 0.0f})
      .onDismiss([this] { closePopup(); });
  popup->content(std::move(list));
  overlay_ = host_->show(std::move(popup));
  open_ = true;
  host_->focus(raw);
  markDirty(DirtyFlag::Paint);
}
void TimePicker::closePopup() {
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
bool TimePicker::onPointerEvent(const PointerEvent &e) {
  if (e.action == PointerAction::Down && primary(e) &&
      bounds().contains(e.position)) {
    setVisualState(ControlVisualState::Focused, true);
    open_ ? closePopup() : openPopup();
    return true;
  }
  return false;
}
bool TimePicker::onKeyEvent(const KeyEvent &e) {
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
bool TimePicker::onTextInput(const TextInputEvent &e) {
  if (e.text.empty())
    return false;
  text_ += e.text;
  validateText();
  return true;
}
AccessibilityActionCapabilities TimePicker::accessibilityActions() const noexcept {
  AccessibilityActionCapabilities actions;
  actions.expandCollapse = host_ != nullptr;
  actions.setValue = true;
  return actions;
}
AccessibilityActionStatus TimePicker::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value) {
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
