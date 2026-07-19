#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/date_time.h"
#include "wui/paint_context.h"
#include "wui/runtime.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"
namespace {
void expect(bool v,const char*m){if(!v)throw std::runtime_error(m);} void ppm(const std::string&p,const std::vector<unsigned char>&v,int w,int h){std::ofstream o(p,std::ios::binary);o<<"P6\n"<<w<<' '<<h<<"\n255\n";for(size_t i=0;i+3<v.size();i+=4){o.put(static_cast<char>(v[i]));o.put(static_cast<char>(v[i+1]));o.put(static_cast<char>(v[i+2]));}}
void render(const std::string& file,float scale){constexpr int lw=780,lh=390;int w=std::lround(lw*scale),h=std::lround(lh*scale);auto canvas=wsc::Canvas::create(wsc::Canvas::Backend::Software,w,h);expect(canvas&&canvas->initializeContext(),"Software canvas");wui::WhatsCanvasTextMeasurer m(*canvas,scale);wui::setTextMeasurer(&m);try{wui::PaintContext c(*canvas,scale);canvas->beginFrame();c.fillRect({0,0,(float)lw,(float)lh},wui::theme().colors.neutralBackground2.rest);c.drawText("Fluent date and time",32,38,24,wui::theme().colors.neutralForeground1,600);wui::Calendar cal;cal.setDisplayedMonth({2024,2,1});cal.setSelectedRange(wui::CivilDate{2024,2,12},wui::CivilDate{2024,2,16});cal.setSelectionMode(wui::CalendarSelectionMode::Range);cal.layout({32,60,276,300});cal.prepare(c);cal.paint(c);wui::DatePicker dp;dp.setValue(wui::CivilDate{2024,2,29});dp.layout({350,90,230,32});dp.prepare(c);dp.paint(c);wui::OverlayHost overlays;wui::TimePicker tp;tp.setValue(wui::CivilTime{9,30,0});tp.bindOverlayHost(overlays);tp.layout({350,144,180,32});expect(tp.bounds().y==144.0f,"time field layout");tp.prepare(c);tp.paint(c);expect(tp.performAccessibilityAction(wui::AccessibilityActionKind::Expand,{})==wui::AccessibilityActionStatus::Succeeded,"time popup");expect(overlays.top()!=nullptr,"time overlay");auto* popup=dynamic_cast<wui::Popup*>(overlays.top()->content.get());expect(popup!=nullptr&&popup->anchor().y==144.0f,"time popup anchor");overlays.layout({0,0,(float)lw,(float)lh});expect(popup->panelBounds().y==180.0f,"time popup placement");overlays.prepare(c);overlays.paint(c);canvas->endFrame();auto pixels=canvas->readPixelsRGBA();expect(!pixels.empty(),"capture");ppm(file,pixels,w,h);}catch(...){wui::setTextMeasurer(nullptr);throw;}wui::setTextMeasurer(nullptr);}
} int main(int argc,char**argv){try{render(argc>1?argv[1]:"fluent_date_time_review.ppm",argc>2?std::stof(argv[2]):1.f);return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
