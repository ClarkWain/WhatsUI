#include "wui/form_feedback.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include "wui/icons.h"
#include "wui/text_input.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui { namespace {
constexpr float kGap=4, kHGap=12, kPad=12, kIcon=20, kMsgGap=8, kActionW=72, kActionH=28;
float widthOf(const std::string& s,const TextStyleToken& t) noexcept { if(const auto* m=textMeasurer()) return m->measureText(s,t.size,t.weight).width; return s.size()*t.size*.56f; }
Color validation(FieldValidationState s,const Theme& t) noexcept { switch(s){case FieldValidationState::Warning:return t.colors.statusWarning;case FieldValidationState::Error:return t.colors.statusDanger;case FieldValidationState::Success:return t.colors.statusSuccess;default:return t.colors.neutralForeground3;} }
Color accent(MessageBarIntent i,const Theme& t) noexcept { switch(i){case MessageBarIntent::Success:return t.colors.statusSuccess;case MessageBarIntent::Warning:return t.colors.statusWarning;case MessageBarIntent::Error:return t.colors.statusDanger;default:return t.colors.statusInfo;} }
Color messageBg(MessageBarIntent i) noexcept { switch(i){case MessageBarIntent::Success:return {237,247,237,255};case MessageBarIntent::Warning:return {255,244,206,255};case MessageBarIntent::Error:return {253,237,238,255};default:return theme().colors.neutralBackground3.rest;} }
Color messageBorder(MessageBarIntent i,const Theme& t) noexcept { switch(i){case MessageBarIntent::Success:return {159,205,159,255};case MessageBarIntent::Warning:return {253,231,156,255};case MessageBarIntent::Error:return {238,172,178,255};default:return t.colors.neutralStroke1;} }
IconName messageIcon(MessageBarIntent intent) noexcept { switch(intent){case MessageBarIntent::Success:return IconName::CheckmarkCircle;case MessageBarIntent::Warning:return IconName::Warning;case MessageBarIntent::Error:return IconName::ErrorCircle;default:return IconName::Info;} }
} // namespace

Field::Field(std::string v):label_(std::move(v)){}
Field& Field::label(std::string v){setLabel(std::move(v));return *this;} void Field::setLabel(std::string v){if(label_!=v){label_=std::move(v);syncControlSemantics();markDirty(DirtyFlag::Layout);}} const std::string& Field::label()const noexcept{return label_;}
Field& Field::hint(std::string v){setHint(std::move(v));return *this;} void Field::setHint(std::string v){if(hint_!=v){hint_=std::move(v);markDirty(DirtyFlag::Layout);}} const std::string& Field::hint()const noexcept{return hint_;}
Field& Field::validationMessage(std::string v){setValidationMessage(std::move(v));return *this;} void Field::setValidationMessage(std::string v){if(validationMessage_!=v){validationMessage_=std::move(v);syncControlSemantics();markDirty(DirtyFlag::Layout);}} const std::string& Field::validationMessage()const noexcept{return validationMessage_;}
Field& Field::validationState(FieldValidationState v)noexcept{setValidationState(v);return *this;} void Field::setValidationState(FieldValidationState v)noexcept{if(validationState_!=v){validationState_=v;syncControlSemantics();markDirty(DirtyFlag::Paint);}} FieldValidationState Field::validationState()const noexcept{return validationState_;}
Field& Field::required(bool v)noexcept{setRequired(v);return *this;} void Field::setRequired(bool v)noexcept{if(required_!=v){required_=v;markDirty(DirtyFlag::Paint);}} bool Field::isRequired()const noexcept{return required_;}
Field& Field::orientation(FieldOrientation v)noexcept{setOrientation(v);return *this;} void Field::setOrientation(FieldOrientation v)noexcept{if(orientation_!=v){orientation_=v;markDirty(DirtyFlag::Layout);}} FieldOrientation Field::orientation()const noexcept{return orientation_;}
Field& Field::enabled(bool v)noexcept{setEnabled(v);return *this;} void Field::setEnabled(bool v)noexcept{if(enabled_==v)return;enabled_=v;if(auto* n=control())if(auto* c=dynamic_cast<ControlNode*>(n))c->setEnabled(v);markDirty(DirtyFlag::Paint);} bool Field::isEnabled()const noexcept{return enabled_;}
Field& Field::control(std::unique_ptr<Node> v){setControl(std::move(v));return *this;} void Field::setControl(std::unique_ptr<Node> v){clearChildren();if(v)appendChild(std::move(v));syncControlSemantics();markDirty(DirtyFlag::Layout);} Node* Field::control()const noexcept{return children().empty()?nullptr:children().front().get();}
void Field::syncControlSemantics(){if(auto* i=dynamic_cast<TextInput*>(control())){if(!label_.empty())i->setAccessibleLabel(label_);i->setInvalid(validationState_==FieldValidationState::Error);i->setEnabled(enabled_);}else if(auto* c=dynamic_cast<ControlNode*>(control()))c->setEnabled(enabled_);} float Field::labelWidth()const noexcept{return widthOf(label_+(required_?" *":""),theme().typography.body1Strong);}
SizeF Field::measure(const Constraints& c)const{const auto&t=theme();auto* n=control();const SizeF cs=n?n->measureWithConstraints(c):SizeF{};const float lh=label_.empty()?0:t.typography.body1Strong.lineHeight,hh=hint_.empty()?0:t.typography.caption1.lineHeight,vh=validationMessage_.empty()?0:t.typography.caption1.lineHeight;if(orientation_==FieldOrientation::Horizontal){const float lw=label_.empty()?0:labelWidth()+kHGap;return c.clamp({lw+cs.width,std::max(lh,cs.height+(hh?kGap+hh:0)+(vh?kGap+vh:0))});}const float gaps=(lh&&cs.height?kGap:0)+(hh?kGap:0)+(vh?kGap:0);return c.clamp({std::max({labelWidth(),cs.width,widthOf(hint_,t.typography.caption1),widthOf(validationMessage_,t.typography.caption1)}),lh+cs.height+hh+vh+gaps});}
void Field::layout(const RectF& b){Node::layout(b);if(auto*n=control()){const auto&t=theme();const float lw=orientation_==FieldOrientation::Horizontal&&!label_.empty()?std::min(b.width,labelWidth()+kHGap):0;const float y=orientation_==FieldOrientation::Vertical&&!label_.empty()?b.y+t.typography.body1Strong.lineHeight+kGap:b.y;const SizeF s=n->measureWithConstraints({0,std::max(0.f,b.width-lw),0,b.height});n->layout({b.x+lw,y,std::max(0.f,b.width-lw),s.height});}clearLayoutDirtyRecursively();}
void Field::paint(PaintContext& c){const auto&t=theme();const auto&ls=t.typography.body1Strong;const auto&hs=t.typography.caption1;auto*n=control();const float lx=bounds().x,head=orientation_==FieldOrientation::Horizontal&&!label_.empty()?std::min(bounds().width,labelWidth()+kHGap):0;const Color fg=enabled_?t.colors.neutralForeground1:t.colors.neutralForegroundDisabled;if(!label_.empty()){RectF b{lx,bounds().y,head?head-kHGap:bounds().width,ls.lineHeight};c.drawText(label_,b.x,c.centeredTextBottom(label_,b,ls.size,ls.weight),ls.size,fg,ls.weight,ls.family);if(required_)c.drawText("*",b.x+widthOf(label_,ls)+t.spacing.horizontal.xs,c.centeredTextBottom("*",b,ls.size,ls.weight),ls.size,t.colors.statusDanger,ls.weight,ls.family);}float y=n?n->bounds().y+n->bounds().height:bounds().y;const float x=bounds().x+head,w=std::max(0.f,bounds().width-head);if(!hint_.empty()){y+=kGap;RectF b{x,y,w,hs.lineHeight};c.drawText(hint_,x,c.centeredTextBottom(hint_,b,hs.size,hs.weight),hs.size,enabled_?t.colors.neutralForeground3:t.colors.neutralForegroundDisabled,hs.weight,hs.family);y+=hs.lineHeight;}if(!validationMessage_.empty()){y+=kGap;RectF b{x,y,w,hs.lineHeight};c.drawText(validationMessage_,x,c.centeredTextBottom(validationMessage_,b,hs.size,hs.weight),hs.size,validation(validationState_,t),hs.weight,hs.family);}ContainerNode::paint(c);clearDirty(DirtyFlag::Paint);}

MessageBar::MessageBar(std::string v):body_(std::move(v)){} MessageBar& MessageBar::title(std::string v){setTitle(std::move(v));return *this;}void MessageBar::setTitle(std::string v){if(title_!=v){title_=std::move(v);markDirty(DirtyFlag::Layout);}}const std::string& MessageBar::title()const noexcept{return title_;}MessageBar& MessageBar::body(std::string v){setBody(std::move(v));return *this;}void MessageBar::setBody(std::string v){if(body_!=v){body_=std::move(v);markDirty(DirtyFlag::Layout);}}const std::string& MessageBar::body()const noexcept{return body_;}MessageBar& MessageBar::intent(MessageBarIntent v)noexcept{setIntent(v);return *this;}void MessageBar::setIntent(MessageBarIntent v)noexcept{if(intent_!=v){intent_=v;markDirty(DirtyFlag::Paint);}}MessageBarIntent MessageBar::intent()const noexcept{return intent_;}MessageBar& MessageBar::multiline(bool v)noexcept{setMultiline(v);return *this;}void MessageBar::setMultiline(bool v)noexcept{if(multiline_!=v){multiline_=v;markDirty(DirtyFlag::Layout);}}bool MessageBar::isMultiline()const noexcept{return multiline_;}
MessageBar& MessageBar::addAction(MessageBarAction v){actions_.push_back(std::move(v));markDirty(DirtyFlag::Layout);return *this;}MessageBar& MessageBar::clearActions(){actions_.clear();markDirty(DirtyFlag::Layout);return *this;}const std::vector<MessageBarAction>& MessageBar::actions()const noexcept{return actions_;}MessageBar& MessageBar::dismissible(bool v)noexcept{setDismissible(v);return *this;}void MessageBar::setDismissible(bool v)noexcept{if(dismissible_!=v){dismissible_=v;markDirty(DirtyFlag::Layout);}}bool MessageBar::isDismissible()const noexcept{return dismissible_;}MessageBar& MessageBar::onDismiss(DismissHandler h){onDismiss_=std::move(h);return *this;}
SizeF MessageBar::measure(const Constraints& c)const{const auto&t=theme();const float trailing=kPad+(dismissible_?kActionH+kMsgGap:0)+(multiline_?0.f:actions_.size()*kActionW);const float chrome=kPad+kIcon+kMsgGap+trailing;const float titleWidth=widthOf(title_,t.typography.body1Strong);const float bodyWidth=widthOf(body_,t.typography.body1);const float inlineGap=!title_.empty()&&!body_.empty()?4.f:0.f;const float available=std::max(1.f,c.maxWidth-chrome);const std::size_t lines=multiline_?std::max<std::size_t>(1,std::size_t(std::ceil((titleWidth+inlineGap+bodyWidth)/available))):1;const float actionRow=multiline_&&!actions_.empty()?kMsgGap+kActionH+kMsgGap:0.f;const float height=multiline_?10.f+lines*t.typography.body1.lineHeight+actionRow+8.f:std::max(36.f,t.typography.body1.lineHeight);return c.clamp({chrome+titleWidth+inlineGap+bodyWidth,height});}void MessageBar::layout(const RectF& b){Node::layout(b);clearLayoutDirtyRecursively();}
RectF MessageBar::dismissBounds()const noexcept{return {bounds().x+bounds().width-kPad-kActionH,bounds().y+kPad,kActionH,kActionH};}RectF MessageBar::actionBounds(std::size_t i)const noexcept{const float r=bounds().x+bounds().width-kPad-(dismissible_?kActionH+kMsgGap:0);return {r-kActionW*float(actions_.size()-i),bounds().y+bounds().height-kPad-kActionH,kActionW,kActionH};}
void MessageBar::paint(PaintContext& c)
{
    const auto& t = theme();
    const Color a = accent(intent_, t);
    const RectF surface = c.snapRectEdges(bounds());
    c.fillStrokeRoundRect(surface, t.radius.medium,
                          c.snapStrokeWidth(t.stroke.thin),
                          messageBg(intent_), messageBorder(intent_, t));
    const RectF icon{surface.x + kPad,
                     multiline_ ? surface.y + kPad
                                : surface.y + (surface.height - kIcon) * 0.5f,
                     kIcon, kIcon};
    drawIcon(c, messageIcon(intent_), icon, a, IconSize::Size20,
             IconStyle::Filled);
    const float right = kPad + (dismissible_ ? kActionH + kMsgGap : 0) +
                        actions_.size() * kActionW;
    const RectF content{
        icon.x + kIcon + kMsgGap, bounds().y + kPad,
        std::max(0.0f, bounds().width -
                           (icon.x + kIcon + kMsgGap - bounds().x) - right),
        bounds().height - kPad * 2};
    const float lineY = multiline_ ? surface.y + 10.0f : surface.y;
    const float lineHeight =
        multiline_ ? t.typography.body1.lineHeight : surface.height;
    float textX = content.x;
    if (!title_.empty()) {
        const RectF box{textX, lineY, content.width, lineHeight};
        c.drawText(title_, box.x,
                   c.centeredTextBottom(title_, box,
                                        t.typography.body1Strong.size,
                                        t.typography.body1Strong.weight),
                   t.typography.body1Strong.size,
                   t.colors.neutralForeground1,
                   t.typography.body1Strong.weight,
                   t.typography.body1Strong.family);
        textX += widthOf(title_, t.typography.body1Strong) +
                 (!body_.empty() ? 4.0f : 0.0f);
    }
    if (!body_.empty()) {
        const RectF box{textX, lineY,
                        std::max(0.0f, content.x + content.width - textX),
                        lineHeight};
        c.drawText(body_, box.x,
                   c.centeredTextBottom(body_, box, t.typography.body1.size,
                                        t.typography.body1.weight),
                   t.typography.body1.size, t.colors.neutralForeground1,
                   t.typography.body1.weight, t.typography.body1.family);
    }
    for (std::size_t index = 0; index < actions_.size(); ++index) {
        const auto box = actionBounds(index);
        c.fillRoundRect(box, t.radius.small,
                        t.colors.neutralBackground1.rest);
        c.drawText(
            actions_[index].label,
            box.x + (box.width -
                     widthOf(actions_[index].label,
                             t.typography.caption1Strong)) /
                        2,
            c.centeredTextBottom(actions_[index].label, box,
                                 t.typography.caption1Strong.size,
                                 t.typography.caption1Strong.weight),
            t.typography.caption1Strong.size, t.colors.brandForeground1,
            t.typography.caption1Strong.weight,
            t.typography.caption1Strong.family);
    }
    if (dismissible_)
        drawIcon(c, IconName::Dismiss, dismissBounds(),
                 t.colors.neutralForeground1, IconSize::Size16);
    clearDirty(DirtyFlag::Paint);
}
bool MessageBar::onPointerEvent(const PointerEvent&e){if(!isEnabled())return false;if(e.action==PointerAction::Down&&e.button==MouseButton::Left){setVisualState(ControlVisualState::Pressed,true);return true;}if(e.action==PointerAction::Up&&e.button==MouseButton::Left){const bool p=(visualStates()&toMask(ControlVisualState::Pressed))!=0;setVisualState(ControlVisualState::Pressed,false);if(!p)return false;if(dismissible_&&dismissBounds().contains(e.position)){dismiss();return true;}for(std::size_t i=0;i<actions_.size();++i)if(actionBounds(i).contains(e.position)){if(actions_[i].onInvoke)actions_[i].onInvoke();return true;}return true;}if(e.action==PointerAction::Enter){setVisualState(ControlVisualState::Hovered,true);return true;}if(e.action==PointerAction::Leave||e.action==PointerAction::Cancel){setVisualState(ControlVisualState::Hovered,false);setVisualState(ControlVisualState::Pressed,false);return true;}return false;}bool MessageBar::onKeyEvent(const KeyEvent&e){if(isEnabled()&&dismissible_&&e.action==KeyAction::Down&&e.keyCode==27){dismiss();return true;}return false;}AccessibilityActionCapabilities MessageBar::accessibilityActions()const noexcept{AccessibilityActionCapabilities a;a.invoke=dismissible_;return a;}AccessibilityActionStatus MessageBar::performAccessibilityAction(AccessibilityActionKind k,std::string_view){if(k!=AccessibilityActionKind::Invoke||!dismissible_)return AccessibilityActionStatus::NotSupported;if(!isEnabled())return AccessibilityActionStatus::ElementNotEnabled;dismiss();return AccessibilityActionStatus::Succeeded;}void MessageBar::dismiss(){if(onDismiss_)onDismiss_();}
} // namespace wui
