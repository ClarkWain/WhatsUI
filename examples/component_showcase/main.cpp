// WhatsUI Component Showcase — a single scrollable page presenting every
// widget with its state, size, and appearance variants side by side.
//
// Design intent
// -------------
// Unlike the maintained Component Gallery, this example intentionally has
// zero navigation, view-model, or router indirection. It is meant to be the
// shortest possible answer to the question "what does WhatsUI look like?".
// The reference visual is `doc/images/fluent_range_matrix_150dpi.png`: a
// densely packed grid of section titles, all-caps subsection captions, and
// rows of widget variants.
//
// Every section lives in this single translation unit; each helper below
// builds one section and returns a view. The theme toggle at the top of the
// window rebuilds the whole tree via `window.setRoot()` scheduled at the next
// frame boundary, so switching light/dark is a real refresh — no local widget
// state is inspected outside of the declarative surface.

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "wui/wui.h"
#include "wui/glfw_platform.h"

using namespace wui;

namespace {

// -----------------------------------------------------------------------------
// Layout constants — kept in one place so the whole showcase reads as a grid.
// -----------------------------------------------------------------------------

constexpr float kPagePadding    = 40.0f;
constexpr float kSectionGap     = 44.0f;
constexpr float kAfterTitleGap  = 24.0f;
constexpr float kSubsectionGap  = 12.0f;
constexpr float kCaptionGap     = 6.0f;
constexpr float kRowGap         = 32.0f;
constexpr float kInnerGap       = 12.0f;

// -----------------------------------------------------------------------------
// Theme-aware helpers. Colors resolve at build time; the whole subtree is
// rebuilt on theme change so these are always fresh.
// -----------------------------------------------------------------------------

Color foreground1()      { return theme().colors.neutralForeground1; }
Color foreground2()      { return theme().colors.neutralForeground2; }
Color foreground3()      { return theme().colors.neutralForeground3; }
Color pageBackground()   { return theme().colors.neutralBackground2.rest; }
Color surfaceBackground(){ return theme().colors.neutralBackground1.rest; }

Text sectionHeading(std::string title)
{
    return Text(std::move(title))
        .size(22)
        .weight(600)
        .color(foreground1());
}

Text subsectionCaption(std::string label)
{
    // Small, all-caps, tracked-out grey label — matches the visual matrix
    // captions from the reference image.
    return Text(std::move(label))
        .size(11)
        .weight(600)
        .color(foreground3());
}

Text variantCaption(std::string label)
{
    return Text(std::move(label))
        .size(12)
        .weight(400)
        .color(foreground2());
}

Text bodyText(std::string content)
{
    return Text(std::move(content)).size(14).color(foreground1());
}

// A titled "cell" — a caption line above a widget. Used to label individual
// variants inside a row (e.g. "Small", "Medium", "Focused").
template <class Child>
Column labeledCell(std::string caption, Child&& widget)
{
    return Column()
        .gap(kCaptionGap)
        .children(variantCaption(std::move(caption)),
                  std::forward<Child>(widget));
}

// -----------------------------------------------------------------------------
// Typography
// -----------------------------------------------------------------------------

Column buildTypographySection()
{
    return Column().gap(kSubsectionGap).children(
        sectionHeading("Typography"),
        subsectionCaption("HEADINGS / BODY / EMPHASIS"),
        Row().gap(kRowGap).children(
            labeledCell("Display",
                        Text("Aa").size(48).weight(700).color(foreground1())),
            labeledCell("Title",
                        Text("Title 1").size(28).weight(600).color(foreground1())),
            labeledCell("Subtitle",
                        Text("Subtitle").size(20).weight(600).color(foreground1())),
            labeledCell("Body",
                        Text("Body regular").size(14).color(foreground1())),
            labeledCell("Body strong",
                        Text("Body strong").size(14).weight(600).color(foreground1())),
            labeledCell("Caption",
                        Text("Caption 1").size(12).color(foreground2()))
        ),
        subsectionCaption("INLINE STYLE / LINK"),
        Row().gap(kRowGap).children(
            labeledCell("Underline",
                        Text("Underlined").size(14).underline().color(foreground1())),
            labeledCell("Strikethrough",
                        Text("Strikethrough").size(14).strikethrough().color(foreground1())),
            labeledCell("Muted",
                        Text("Secondary text").size(14).color(foreground2())),
            labeledCell("Link",
                        Link("whatsui.dev").href("https://example.invalid"))
        )
    );
}

// -----------------------------------------------------------------------------
// Buttons
// -----------------------------------------------------------------------------

Column buildButtonsSection()
{
    return Column().gap(kSubsectionGap).children(
        sectionHeading("Buttons"),
        subsectionCaption("APPEARANCE"),
        Row().gap(kInnerGap).align(Alignment::Center).children(
            Button("Primary").appearance(ButtonAppearance::Primary),
            Button("Secondary").appearance(ButtonAppearance::Secondary),
            Button("Outline").appearance(ButtonAppearance::Outline),
            Button("Subtle").appearance(ButtonAppearance::Subtle),
            Button("Transparent").appearance(ButtonAppearance::Transparent),
            Button("Danger").appearance(ButtonAppearance::Danger)
        ),
        subsectionCaption("SIZE"),
        Row().gap(kInnerGap).align(Alignment::Center).children(
            Button("Small").size(ButtonSize::Small),
            Button("Medium").size(ButtonSize::Medium),
            Button("Large").size(ButtonSize::Large)
        ),
        subsectionCaption("SHAPE"),
        Row().gap(kInnerGap).align(Alignment::Center).children(
            Button("Rounded").shape(ButtonShape::Rounded),
            Button("Square").shape(ButtonShape::Square),
            Button("Circular").shape(ButtonShape::Circular).icon(IconName::Add).iconOnly()
        ),
        subsectionCaption("ICON / COMPOUND / TOGGLE"),
        Row().gap(kInnerGap).align(Alignment::Center).children(
            Button("Add").icon(IconName::Add).appearance(ButtonAppearance::Primary),
            Button("Delete").icon(IconName::Delete).iconPosition(ButtonIconPosition::Before),
            Button("Next").icon(IconName::ChevronRight).iconPosition(ButtonIconPosition::After),
            IconButton(IconName::Search, "Search"),
            IconButton(IconName::Edit, "Edit").iconStyle(IconStyle::Filled),
            CompoundButton("Sign in", "Continue with Microsoft")
                .appearance(ButtonAppearance::Primary),
            ToggleButton("Pin").icon(IconName::Star)
        )
    );
}

// -----------------------------------------------------------------------------
// Selection controls
// -----------------------------------------------------------------------------

Column buildSelectionSection()
{
    return Column().gap(kSubsectionGap).children(
        sectionHeading("Selection controls"),
        subsectionCaption("CHECKBOX STATE / SIZE / SHAPE"),
        Row().gap(kRowGap).align(Alignment::Center).children(
            labeledCell("Unchecked", Checkbox("Option")),
            labeledCell("Checked",   Checkbox("Option").checked(true)),
            labeledCell("Mixed",     Checkbox("Option").mixed(true)),
            labeledCell("Disabled",  Checkbox("Option").checked(true).enabled(false)),
            labeledCell("Large",     Checkbox("Larger").checked(true).size(CheckboxSize::Large)),
            labeledCell("Circular",  Checkbox("Circular").checked(true).shape(CheckboxShape::Circular))
        ),
        subsectionCaption("RADIO GROUP / LAYOUT"),
        Row().gap(kRowGap).children(
            labeledCell("Vertical",
                RadioGroup()
                    .layout(RadioGroupLayout::Vertical)
                    .option("first", "First")
                    .option("second", "Second")
                    .option("third", "Unavailable", /*enabled*/ false)
                    .value("first")),
            labeledCell("Horizontal",
                RadioGroup()
                    .layout(RadioGroupLayout::Horizontal)
                    .option("alpha", "Alpha")
                    .option("beta", "Beta")
                    .option("gamma", "Gamma")
                    .value("beta"))
        ),
        subsectionCaption("SWITCH SIZE / STATE / LABEL POSITION"),
        Row().gap(kRowGap).align(Alignment::Center).children(
            labeledCell("Off",       Switch("Off")),
            labeledCell("On",        Switch("On").on(true)),
            labeledCell("Small",     Switch("Small").size(SwitchSize::Small).on(true)),
            labeledCell("Disabled",  Switch("Off").enabled(false)),
            labeledCell("Before",    Switch("Before").labelPosition(SwitchLabelPosition::Before).on(true)),
            labeledCell("Required",
                Switch("Notifications *")
                    .required(true)
                    .on(true))
        )
    );
}

// -----------------------------------------------------------------------------
// Range & progress
// -----------------------------------------------------------------------------

Column buildRangeSection(State<float>& sliderValue,
                        State<float>& progressValue,
                        State<float>& ratingValue)
{
    return Column().gap(kSubsectionGap).children(
        sectionHeading("Range and progress"),
        subsectionCaption("SLIDER SIZE / STATE / ORIENTATION"),
        Row().gap(kRowGap).children(
            labeledCell("Medium",
                Slider(0.0f, 100.0f, 30.0f)
                    .bind(sliderValue)
                    .accessibleLabel("Volume")),
            labeledCell("Small",
                Slider(0.0f, 100.0f, 70.0f)
                    .size(SliderSize::Small)),
            labeledCell("Disabled",
                Slider(0.0f, 100.0f, 45.0f).enabled(false))
        ),
        subsectionCaption("PROGRESS COLOR / SHAPE / THICKNESS / INDETERMINATE"),
        Row().gap(kRowGap).children(
            labeledCell("Brand",
                ProgressBar(0.0f, 1.0f, 0.4f).bind(progressValue)),
            labeledCell("Success",
                ProgressBar(0.0f, 1.0f, 0.6f).color(ProgressBarColor::Success)),
            labeledCell("Warning",
                ProgressBar(0.0f, 1.0f, 0.6f).color(ProgressBarColor::Warning)),
            labeledCell("Error",
                ProgressBar(0.0f, 1.0f, 0.6f).color(ProgressBarColor::Error)),
            labeledCell("Large",
                ProgressBar(0.0f, 1.0f, 0.75f).thickness(ProgressBarThickness::Large)),
            labeledCell("Indeterminate",
                ProgressBar().indeterminate(true))
        ),
        subsectionCaption("RATING SIZE / COLOR / SHAPE / READ-ONLY"),
        Row().gap(kRowGap).align(Alignment::Center).children(
            labeledCell("Interactive",
                Rating(3.5f, 5).bind(ratingValue)),
            labeledCell("Large",
                Rating(4.0f, 5).size(RatingSize::Large)),
            labeledCell("Read-only",
                RatingDisplay(std::optional<float>(4.2f), 5).count(1281).compact(true))
        )
    );
}

// -----------------------------------------------------------------------------
// Dividers
// -----------------------------------------------------------------------------

Column buildDividerSection()
{
    return Column().gap(kSubsectionGap).children(
        sectionHeading("Dividers"),
        subsectionCaption("APPEARANCE"),
        Column().gap(8.0f).children(
            Divider().content("Default").appearance(DividerAppearance::Default),
            Divider().content("Subtle").appearance(DividerAppearance::Subtle),
            Divider().content("Brand").appearance(DividerAppearance::Brand),
            Divider().content("Strong").appearance(DividerAppearance::Strong)
        ),
        subsectionCaption("CONTENT ALIGNMENT"),
        Column().gap(8.0f).children(
            Divider().content("Start").contentAlignment(DividerContentAlignment::Start),
            Divider().content("Center").contentAlignment(DividerContentAlignment::Center),
            Divider().content("End").contentAlignment(DividerContentAlignment::End)
        )
    );
}

// -----------------------------------------------------------------------------
// Text inputs
// -----------------------------------------------------------------------------

Column buildInputsSection()
{
    return Column().gap(kSubsectionGap).children(
        sectionHeading("Text inputs"),
        subsectionCaption("APPEARANCE / SIZE / STATE"),
        Row().gap(kRowGap).children(
            labeledCell("Outline",
                TextField("Placeholder")
                    .appearance(InputAppearance::Outline)),
            labeledCell("Underline",
                TextField("Underline")
                    .appearance(InputAppearance::Underline)),
            labeledCell("Filled darker",
                TextField("Search")
                    .appearance(InputAppearance::FilledDarker)),
            labeledCell("Filled lighter",
                TextField("Search")
                    .appearance(InputAppearance::FilledLighter)),
            labeledCell("Small",
                TextField("Small").size(InputSize::Small)),
            labeledCell("Large",
                TextField("Large").size(InputSize::Large))
        ),
        subsectionCaption("SEARCH / MULTILINE / FIELD"),
        Row().gap(kRowGap).children(
            labeledCell("Search",
                SearchField("Search components")),
            labeledCell("Multiline",
                TextArea("Write something…")
                    .rows(3)),
            labeledCell("Field with hint",
                Field("Email")
                    .hint("We will never share your email.")
                    .required(true)
                    .control(TextField("name@example.com"))),
            labeledCell("Field with error",
                Field("Password")
                    .required(true)
                    .validationState(FieldValidationState::Error)
                    .validationMessage("Password must be at least 8 characters.")
                    .control(TextField("").invalid(true)))
        )
    );
}

// -----------------------------------------------------------------------------
// List, combobox, dropdown — collections that need an overlay host
// -----------------------------------------------------------------------------

Column buildCollectionInputsSection(OverlayHost& host)
{
    std::vector<Option> planetOptions;
    planetOptions.push_back(Option{"mercury", "Mercury"});
    planetOptions.push_back(Option{"venus", "Venus"});
    planetOptions.push_back(Option{"earth", "Earth"});
    planetOptions.push_back(Option{"mars", "Mars"});

    return Column().gap(kSubsectionGap).children(
        sectionHeading("Collection inputs"),
        subsectionCaption("LISTBOX / COMBOBOX / DROPDOWN"),
        Row().gap(kRowGap).children(
            labeledCell("ListBox (multiple)",
                ListBox(planetOptions).selectedIndex(2).multiple(true)),
            labeledCell("Combobox",
                Combobox("Choose planet")
                    .option(Option{"mercury", "Mercury"})
                    .option(Option{"venus", "Venus"})
                    .option(Option{"earth", "Earth"})
                    .option(Option{"mars", "Mars"})
                    .selectedIndex(2)
                    .overlayHost(host)),
            labeledCell("Dropdown",
                Dropdown("Select an option")
                    .option(Option{"low", "Low"})
                    .option(Option{"medium", "Medium"})
                    .option(Option{"high", "High"})
                    .selectedIndex(1)
                    .overlayHost(host))
        )
    );
}

// -----------------------------------------------------------------------------
// Feedback (message bar, toast, spinner)
// -----------------------------------------------------------------------------

Column buildFeedbackSection()
{
    return Column().gap(kSubsectionGap).children(
        sectionHeading("Feedback"),
        subsectionCaption("MESSAGE BAR / INTENT"),
        Column().gap(8.0f).children(
            MessageBar("Your changes have been saved.")
                .title("Success")
                .intent(MessageBarIntent::Success)
                .dismissible(true),
            MessageBar("Deployment finished in 34s.")
                .title("Info")
                .intent(MessageBarIntent::Info),
            MessageBar("Storage is almost full.")
                .title("Warning")
                .intent(MessageBarIntent::Warning),
            MessageBar("Sync failed — check your connection.")
                .title("Error")
                .intent(MessageBarIntent::Error)
                .multiline(true)
        ),
        subsectionCaption("SPINNER SIZE / LABEL POSITION"),
        Row().gap(kRowGap).align(Alignment::Center).children(
            labeledCell("Tiny",       Spinner().size(SpinnerSize::Tiny)),
            labeledCell("Small",      Spinner().size(SpinnerSize::Small)),
            labeledCell("Medium",     Spinner("Loading").size(SpinnerSize::Medium)),
            labeledCell("Large",      Spinner().size(SpinnerSize::Large)),
            labeledCell("Below",      Spinner("Working…").labelPosition(SpinnerLabelPosition::Below))
        )
    );
}

// -----------------------------------------------------------------------------
// Badges, avatars, personas
// -----------------------------------------------------------------------------

Column buildIdentitySection()
{
    return Column().gap(kSubsectionGap).children(
        sectionHeading("Badges, avatars and personas"),
        subsectionCaption("BADGE APPEARANCE / COLOR / SHAPE"),
        Row().gap(kInnerGap).align(Alignment::Center).children(
            Badge("Filled").appearance(BadgeAppearance::Filled).color(BadgeColor::Brand),
            Badge("Ghost").appearance(BadgeAppearance::Ghost).color(BadgeColor::Brand),
            Badge("Outline").appearance(BadgeAppearance::Outline).color(BadgeColor::Neutral),
            Badge("Tint").appearance(BadgeAppearance::Tint).color(BadgeColor::Success),
            Badge("Warning").appearance(BadgeAppearance::Filled).color(BadgeColor::Warning),
            Badge("Danger").appearance(BadgeAppearance::Filled).color(BadgeColor::Danger),
            Badge("Circle").shape(BadgeShape::Circular).color(BadgeColor::Brand)
        ),
        subsectionCaption("COUNTER / PRESENCE"),
        Row().gap(kRowGap).align(Alignment::Center).children(
            labeledCell("0 (shown)",     CounterBadge(0).showZero(true)),
            labeledCell("12",            CounterBadge(12)),
            labeledCell("99+",           CounterBadge(150).max(99)),
            labeledCell("Available",     PresenceBadge(PresenceStatus::Available)),
            labeledCell("Busy",          PresenceBadge(PresenceStatus::Busy)),
            labeledCell("Do not disturb",PresenceBadge(PresenceStatus::DoNotDisturb)),
            labeledCell("Away",          PresenceBadge(PresenceStatus::Away)),
            labeledCell("Offline",       PresenceBadge(PresenceStatus::Offline))
        ),
        subsectionCaption("AVATAR SIZE / SHAPE / COLOR"),
        Row().gap(kInnerGap).align(Alignment::Center).children(
            Avatar("Ava Chen",    AvatarSize::Size20),
            Avatar("Ben Turing",  AvatarSize::Size28),
            Avatar("Cara Diaz",   AvatarSize::Size36),
            Avatar("Devon Ito",   AvatarSize::Size48),
            Avatar("Elena Ruiz",  AvatarSize::Size72).shape(AvatarShape::Square),
            Avatar("Farah Kim",   AvatarSize::Size56).color(AvatarColor::Plum).active(true)
        ),
        subsectionCaption("AVATAR GROUP"),
        Row().gap(kRowGap).align(Alignment::Center).children(
            AvatarGroup()
                .avatar("Ava")
                .avatar("Ben")
                .avatar("Cara")
                .avatar("Devon")
                .avatar("Elena")
                .maxVisible(3)
        ),
        subsectionCaption("PERSONA"),
        Row().gap(kRowGap).align(Alignment::Start).children(
            Persona("Kat Larsson")
                .primaryText("Kat Larsson")
                .secondaryText("Design Systems")
                .tertiaryText("Available")
                .presence(PresenceStatus::Available)
                .size(PersonaSize::Medium),
            Persona("Miguel Ruiz")
                .primaryText("Miguel Ruiz")
                .secondaryText("Principal Engineer")
                .tertiaryText("In a meeting")
                .presence(PresenceStatus::Busy)
                .size(PersonaSize::Large),
            Persona("Nia Osei")
                .primaryText("Nia Osei")
                .secondaryText("PM")
                .presence(PresenceStatus::Away)
                .size(PersonaSize::Small)
        )
    );
}

// -----------------------------------------------------------------------------
// Surfaces (card)
// -----------------------------------------------------------------------------

Column buildSurfacesSection()
{
    auto sampleCard = [](std::string title, CardAppearance appearance) {
        return Card()
            .appearance(appearance)
            .children(
                CardHeader(std::move(title), "Two lines of secondary text describing the card."),
                Text("Cards group related information. Any child view can be placed here — text, "
                     "images, buttons, or entire subtrees.")
                    .size(13)
                    .color(foreground2()),
                Row().gap(8.0f).children(
                    Button("Learn more").appearance(ButtonAppearance::Primary),
                    Button("Dismiss").appearance(ButtonAppearance::Subtle)
                )
            );
    };

    return Column().gap(kSubsectionGap).children(
        sectionHeading("Surfaces"),
        subsectionCaption("CARD APPEARANCE"),
        Row().gap(kRowGap).align(Alignment::Start).children(
            sampleCard("Filled",             CardAppearance::Filled),
            sampleCard("Filled alternative", CardAppearance::FilledAlternative),
            sampleCard("Outline",            CardAppearance::Outline),
            sampleCard("Subtle",             CardAppearance::Subtle)
        )
    );
}

// -----------------------------------------------------------------------------
// Navigation (toolbar, breadcrumb, tab list)
// -----------------------------------------------------------------------------

Column buildNavigationSection()
{
    return Column().gap(kSubsectionGap).children(
        sectionHeading("Navigation"),
        subsectionCaption("TOOLBAR"),
        Toolbar()
            .item("File")
            .item("Edit")
            .item("View")
            .item("Selection")
            .item("Go")
            .item("Help"),
        subsectionCaption("BREADCRUMB"),
        Breadcrumb()
            .item("WhatsUI")
            .item("Examples")
            .item("component_showcase")
            .item("main.cpp", /*current*/ true),
        subsectionCaption("TAB LIST"),
        TabList()
            .tab("home",    "Home")
            .tab("docs",    "Documentation")
            .tab("gallery", "Gallery")
            .tab("about",   "About")
            .value("home")
    );
}

// -----------------------------------------------------------------------------
// Collections (accordion, tree, list view)
// -----------------------------------------------------------------------------

Column buildCollectionsSection()
{
    auto makeItem = [](std::size_t index) {
        ListViewNode::Item item;
        static const char* names[] = {
            "Ada Lovelace",       "Alan Turing",       "Grace Hopper",
            "Donald Knuth",       "Barbara Liskov",    "Edsger Dijkstra",
            "Margaret Hamilton",  "Ken Thompson",      "Leslie Lamport",
            "Radia Perlman"
        };
        item.label = names[index % (sizeof(names) / sizeof(names[0]))];
        return item;
    };

    return Column().gap(kSubsectionGap).children(
        sectionHeading("Collections"),
        subsectionCaption("ACCORDION"),
        Accordion()
            .expandMode(AccordionExpandMode::Multiple)
            .item("Getting started",
                  "Install WhatsUI, add it as a submodule, or consume it via CMake.")
            .item("Rendering",
                  "Use WhatsCanvas software rendering for deterministic tests, or OpenGL "
                  "for interactive windows.")
            .item("Accessibility",
                  "Windows UI Automation is wired for the standard control patterns."),
        subsectionCaption("TREE"),
        Tree()
            .item("root",     "Project")
            .item("src",      "  src")
            .item("include",  "  include")
            .item("tests",    "  tests")
            .item("examples", "  examples")
            .maxVisibleItems(6),
        subsectionCaption("LIST VIEW (VIRTUALIZED PROVIDER)"),
        Box().width(280.0f).children(
            ListView()
                .itemProvider(60, makeItem)
                .selectedIndex(2)
        )
    );
}

// -----------------------------------------------------------------------------
// Top bar — title on the left, theme toggle on the right.
// -----------------------------------------------------------------------------

Row buildHeader(bool currentDark,
                std::function<void(bool)> onThemeToggle)
{
    return Row()
        .gap(kInnerGap)
        .align(Alignment::Center)
        .padding(InsetsF{0, 0, kAfterTitleGap, 0})
        .children(
            Column().gap(4.0f).children(
                Text("WhatsUI Component Showcase")
                    .size(28)
                    .weight(700)
                    .color(foreground1()),
                Text("Every declarative widget, in one scrollable page. Toggle the theme "
                     "to see how tokens propagate.")
                    .size(13)
                    .color(foreground2())
            ),
            Spacer(9999.0f, 0.0f),
            Text(currentDark ? "Dark" : "Light")
                .size(12)
                .weight(600)
                .color(foreground2()),
            Switch("Dark mode")
                .labelPosition(SwitchLabelPosition::Before)
                .on(currentDark)
                .onChange(std::move(onThemeToggle))
        );
}

// -----------------------------------------------------------------------------
// Root builder — assembles the header, all sections, and the ScrollView.
// -----------------------------------------------------------------------------

// Forward declaration so the theme-toggle callback can request a rebuild.
std::unique_ptr<Node> buildRoot(UiWindow& window,
                                std::shared_ptr<bool> darkMode);

std::unique_ptr<Node> buildRoot(UiWindow& window,
                                std::shared_ptr<bool> darkMode)
{
    // Interactive widget state — persists only within one root instance. It is
    // deliberately re-created on every theme rebuild so the showcase always
    // starts from the same reference values a reader would see in the docs.
    auto sliderValue   = std::make_shared<State<float>>(30.0f);
    auto progressValue = std::make_shared<State<float>>(0.4f);
    auto ratingValue   = std::make_shared<State<float>>(3.5f);

    auto& host = window.overlayHost();

    auto onToggle = [&window, darkMode](bool on) {
        *darkMode = on;
        wui::setTheme(on ? wui::fluentDarkTheme() : wui::Theme{});
        // Defer setRoot until the current event dispatch has unwound. This is
        // the same contract described in wui/scheduler.h: state changes may
        // request structural updates from inside a handler as long as they
        // are queued for the next frame boundary rather than executed inline.
        wui::scheduleStructuralUpdate(&window, [&window, darkMode] {
            window.setRoot(buildRoot(window, darkMode));
        });
    };

    return Box()
        .background(pageBackground())
        .children(
            ScrollView().content(
                Box()
                    .padding(kPagePadding)
                    .children(
                        Column()
                            .gap(kSectionGap)
                            .children(
                                buildHeader(*darkMode, onToggle),
                                buildTypographySection(),
                                buildButtonsSection(),
                                buildSelectionSection(),
                                buildRangeSection(*sliderValue, *progressValue, *ratingValue),
                                buildDividerSection(),
                                buildInputsSection(),
                                buildCollectionInputsSection(host),
                                buildFeedbackSection(),
                                buildIdentitySection(),
                                buildSurfacesSection(),
                                buildNavigationSection(),
                                buildCollectionsSection(),
                                // Bottom spacer so the last section can scroll
                                // fully above the window edge.
                                Spacer(0.0f, kPagePadding)
                            )
                    )
            )
        )
        .build();
}

} // namespace

int main()
{
    try {
        auto darkMode = std::make_shared<bool>(false);

        std::fprintf(stderr, "[showcase] entering runGlfwApp\n");
        const int rc = runGlfwApp(
            "WhatsUI - Component Showcase",
            {1400.0f, 900.0f},
            [darkMode](UiWindow& window) -> std::unique_ptr<Node> {
                std::fprintf(stderr, "[showcase] factory called\n");
                wui::setTheme(*darkMode ? wui::fluentDarkTheme() : wui::Theme{});
                auto root = buildRoot(window, darkMode);
                std::fprintf(stderr, "[showcase] factory produced root=%p\n",
                             static_cast<void*>(root.get()));
                return root;
            });
        std::fprintf(stderr, "[showcase] runGlfwApp returned rc=%d\n", rc);
        return rc;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "[showcase] fatal: %s\n", ex.what());
        return 2;
    } catch (...) {
        std::fprintf(stderr, "[showcase] fatal: unknown exception\n");
        return 3;
    }
}
