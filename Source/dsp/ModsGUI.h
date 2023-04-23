#pragma once
#include <chrono>
#include <thread>
#include "Wavetable.h"
#include "Modulator.h"
#include "../NELG.h"

namespace modSys6
{
    namespace gui
    {
        template<size_t WTSize, size_t NumTables, size_t NumWaveCycles, size_t NumRects>
        struct WavetableView :
            public Comp
        {
            using Tables = dsp::Wavetable3D<double, WTSize, NumTables>;

            WavetableView(Utils& u, juce::String&& _tooltip, const Tables& _tables) :
                Comp(u, std::move(_tooltip), CursorType::Default),
                tables(_tables),
                tablesPhase(0.f)
            {}
            void update(float _tablesPhase)
            {
                if (tablesPhase != _tablesPhase)
                {
                    tablesPhase = _tablesPhase;
                    repaint();
                }
            }
        protected:
            const Tables& tables;
            float tablesPhase;

            void paint(juce::Graphics& g) override
            {
                const auto thicc = Shared::shared.thicc;
                const auto bounds = getLocalBounds().toFloat().reduced(thicc);
                const auto rad = bounds.getHeight() * .5f;
                const auto midY = bounds.getY() + rad;
                static constexpr float NumRectsInv = 1.f / static_cast<float>(NumRects);
                static constexpr float NumWaveCyclesF = static_cast<float>(NumWaveCycles);
                const auto WRatio = bounds.getWidth() * NumRectsInv;
                for (auto i = 0; i < NumRects; ++i)
                {
                    const auto iF = static_cast<float>(i);
                    const auto iX = iF * NumRectsInv;
                    auto window = std::sin(iX * pi);
                    window *= window; window *= window;
                    auto tablePhase = iX * NumWaveCyclesF;
                    while (tablePhase >= 1.f)
                        --tablePhase;
                    const auto smpl = static_cast<float>(tables(tablesPhase, tablePhase)) * -1.f;
                    const auto col = juce::Colours::transparentBlack
                        .interpolatedWith(Shared::shared.colour(ColourID::Mod), window);
                    g.setColour(col);
                    {
                        const auto x = bounds.getX() + iX * bounds.getWidth();
                        const auto w = WRatio;
                        auto y = midY;
                        auto bottom = midY + smpl * rad;
                        if (bottom < y)
                            std::swap(y, bottom);
                        const auto h = bottom - y;
                        g.fillRect(x, y, w, h);
                    }
                }
                //g.drawRoundedRectangle(bounds, thicc, thicc);
            }
        };

        struct ModCompPerlin :
            public Comp,
            public juce::Timer
        {
            enum
            {
                RateHz,
                RateBeats,
                Oct,
                Width,
                RateType,
                Phase,
                Shape,
                RandType,
                NumParams
            };

            ModCompPerlin(Utils& u, std::vector<Paramtr*>& modulatables, int mOff = 0) :
                Comp(u, "", CursorType::Default),
                juce::Timer(),
                layout
                (
                    { 1, 13, 21, 34, 1 },
                    { 13, 21 }
                ),
                params
                {
                    Paramtr(u, "Rate", "The rate of the perlin noise mod in hz.", withOffset(PID::Perlin0RateHz, mOff), modulatables),
					Paramtr(u, "Rate", "The rate of the perlin noise mod in beats.", withOffset(PID::Perlin0RateBeats, mOff), modulatables),
                    Paramtr(u, "Oct", "More octaves add complexity to the signal.", withOffset(PID::Perlin0Octaves, mOff), modulatables),
                    Paramtr(u, "Width", "This parameter adds a phase offset to the right channel.", withOffset(PID::Perlin0Width, mOff), modulatables),
					Paramtr(u, "Temposync", "Switch between the rate units, free running (hz) or temposync (beats).", withOffset(PID::Perlin0RateType, mOff), modulatables, ParameterType::Switch),
					Paramtr(u, "Phase", "Apply a phase shift to the signal.", withOffset(PID::Perlin0Phase, mOff), modulatables),
					Paramtr(u, "Shape", "The perlin noise mod can have 3 shapes. Steppy, linear and round.", withOffset(PID::Perlin0Shape, mOff), modulatables),
					Paramtr(u, "Proc", "Every noise segment corresponds to a distinct combination of rate, bpm and transport info.", withOffset(PID::Perlin0RandType, mOff), modulatables, ParameterType::Switch)
                }
            {
                for (auto& p : params)
                {
                    addAndMakeVisible(p);
                }

				using Stroke = juce::PathStrokeType;
				using Path = juce::Path;

                params[RateType].onPaint = [&](juce::Graphics& g)
                {
					auto thicc = Shared::shared.thicc;
                    const auto thicc3 = thicc * 3.f;
                    const auto bounds = maxQuadIn(params[RateType].getLocalBounds().toFloat()).reduced(thicc3);
                    Stroke stroke(thicc, Stroke::JointStyle::beveled, Stroke::EndCapStyle::butt);

                    const auto x = bounds.getX();
                    const auto y = bounds.getY();
                    const auto w = bounds.getWidth();

                    const auto circleW = w * .2f;
                    const auto circleX = x + w * .3f;
                    const auto circleY = y + w - circleW;

                    g.fillEllipse(circleX, circleY, circleW, circleW);

                    const auto lineX = circleX + circleW - thicc * .5f;
                    const auto lineY0 = y;
                    const auto lineY1 = circleY + circleW * .5f;

                    g.drawLine(lineX, lineY0, lineX, lineY1, thicc);

                    const auto arcX0 = lineX;
                    const auto arcY0 = lineY0;
                    const auto arcX2 = lineX + w * .2f;
                    const auto arcY2 = lineY0 + w * .3f;
                    const auto arcX1 = lineX + w * .3f;
                    const auto arcY1 = lineY0 + w * .1f;

                    Path arc;
                    arc.startNewSubPath(arcX0, arcY0);
                    arc.cubicTo(arcX0, arcY0, arcX1, arcY1, arcX2, arcY2);
                    g.strokePath(arc, stroke);
                };
                params[RateType].targetToggleState = 1;

                params[RandType].onPaint = [&](juce::Graphics& g)
                {
					g.drawFittedText("P", params[RandType].getLocalBounds(), juce::Justification::centred, 1);
                };
				params[RandType].targetToggleState = 1;

                startTimerHz(4);
            }
            
            void activate(ParamtrRandomizer& randomizer)
            {
                for (auto& p : params)
                    randomizer.add(&p);
                setVisible(true);
            }
            
        protected:
            nelG::Layout layout;
            std::array<Paramtr, NumParams> params;

            void mouseEnter(const juce::MouseEvent& evt) override
            {
                Comp::mouseEnter(evt);
            }

            void paint(juce::Graphics&) override
            {
            }

            void resized() override
            {
                layout.setBounds(getLocalBounds().toFloat());
                
                layout.place(params[RateHz], 2, 0, 1, 2);
                layout.place(params[RateBeats], 2, 0, 1, 2);
                // perlin params:
                {
                    const auto area = layout(3, 0, 1, 1);
                    const auto w = area.getWidth();
                    const auto h = area.getHeight();
                    auto x = area.getX();
                    auto y = area.getY();

                    auto buttonWidth = w / 3.f;
                    params[Shape].setBounds(juce::Rectangle<float>(x, y, buttonWidth, h).toNearestInt());
                    x += buttonWidth;
                    params[Shape].setBounds(juce::Rectangle<float>(x, y, buttonWidth, h).toNearestInt());
                    x += buttonWidth;
                    params[Shape].setBounds(juce::Rectangle<float>(x, y, buttonWidth, h).toNearestInt());
                }
                {
                    const auto area = layout(3, 1, 1, 1);
                    const auto w = area.getWidth();
                    const auto knobW = w / 3.f;
                    auto x = area.getX();

                    params[Oct].setBounds(juce::Rectangle<float>(x, area.getY(), knobW, area.getHeight()).toNearestInt());
                    x += knobW;
                    params[Phase].setBounds(juce::Rectangle<float>(x, area.getY(), knobW, area.getHeight()).toNearestInt());
                    x += knobW;
                    params[Width].setBounds(juce::Rectangle<float>(x, area.getY(), knobW, area.getHeight()).toNearestInt());
                }
                //layout.place(seed, 1, 1, 1, 1);
                {
                    const auto area = layout(1, 1, 1, 1);
                    const auto w = area.getWidth();
                    const auto h = area.getHeight();
                    auto x = area.getX();
                    auto y = area.getY();

                    const auto buttonW = w * .5f;

                    params[RateType].setBounds(maxQuadIn(juce::Rectangle<float>(x, y, w - buttonW, h)).toNearestInt());
                    x += buttonW;
                    params[RandType].setBounds(maxQuadIn(juce::Rectangle<float>(x, y, w - buttonW, h)).toNearestInt());
                }
            }

            void timerCallback() override
            {
                bool isTempoSync = utils.getParam(params[RateType].getPID())->getValueSum() > .5f;
                params[RateBeats].setVisible(isTempoSync);
                params[RateHz].setVisible(!isTempoSync);
            }
        };

        class ModCompAudioRate :
            public Comp,
            public juce::Timer
        {
            enum { Oct, Semi, Fine, Width, RetuneSpeed, Atk, Dcy, Sus, Rls, NumParams };

            struct ADSRView :
                public Comp
            {
                ADSRView(Utils& u, int _adsrViewType) :
                    Comp(u, ""),
                    onClick(),
                    adsrViewType(_adsrViewType)
                {
                    setBufferedToImage(false);
                }

                virtual void update(bool) = 0;

                std::function<void()> onClick;
                int adsrViewType;

            protected:
                void mouseUp(const juce::MouseEvent& evt) override
                {
                    if (!evt.mouseWasDraggedSinceMouseDown())
                        if(onClick != nullptr)
                            onClick();
                }
            };

            struct ADSRRel :
                public ADSRView
            {
                enum { A, D, S, R };

                ADSRRel(Utils& u, PID _a, PID _d, PID _s, PID _r) :
                    ADSRView(u, 0),
                    params
                    {
                        u.getParam(_a),
                        u.getParam(_d),
                        u.getParam(_s),
                        u.getParam(_r)
                    },
                    vals { -1.f, -1.f, -1.f, -1.f }
                {
                }
                void update(bool forced = false) override
                {
                    const auto atk = params[A]->getValue();
                    const auto dcy = params[D]->getValue();
                    const auto sus = params[S]->getValue();
                    const auto rls = params[R]->getValue();
                    if (forced || vals[A] != atk || vals[D] != dcy || vals[S] != sus || vals[R] != rls)
                    {
                        vals[A] = atk;
                        vals[D] = dcy;
                        vals[S] = sus;
                        vals[R] = rls;
                        updatePath();
                    }
                }
                void updatePath()
                {
                    const auto thicc = Shared::shared.thicc;
                    const auto bounds = getLocalBounds().toFloat().reduced(thicc);

                    path.clear();
                    path.startNewSubPath(bounds.getBottomLeft());

                    const auto atk = vals[A];
                    const auto dcy = vals[D];
                    const auto sus = vals[S];
                    const auto rls = vals[R];

                    const auto max = atk + dcy + rls;

                    if (max != 0.f)
                    {
                        const auto maxInv = 1.f / max;

                        // attack state
                        const auto atkRel = atk * maxInv;
                        const auto atkX = bounds.getX() + bounds.getWidth() * atkRel;
                        path.lineTo(
                            atkX,
                            bounds.getY()
                        );

                        // decay state (to sustain)
                        const auto dcyRel = dcy * maxInv;
                        const auto dcyX = atkX + bounds.getWidth() * dcyRel;
                        const auto susY = bounds.getBottom() - bounds.getHeight() * sus;
                        path.lineTo(
                            dcyX,
                            susY
                        );
                    }
                    else
                    {
                        const auto susY = bounds.getBottom() - bounds.getHeight() * sus;
                        path.lineTo(bounds.getX(), susY);
                        path.lineTo(bounds.getRight(), susY);
                    }

                    path.lineTo(bounds.getBottomRight());
                    path.closeSubPath();
                    repaint();
                }
            protected:
                std::array<Param*, 4> params;
                std::array<float, 4> vals;

                juce::Path path;

                void resized() override
                {
                    update(true);
                }

                void paint(juce::Graphics& g) override
                {
                    const auto thicc = Shared::shared.thicc;
                    g.setColour(Shared::shared.colour(ColourID::Interact));
                    g.strokePath(path, juce::PathStrokeType(thicc));
                }
            };

            struct ADSRAbs :
                public ADSRView
            {
                enum { A, D, S, R };

                ADSRAbs(Utils& u, PID _a, PID _d, PID _s, PID _r) :
                    ADSRView(u, 1),
                    params
                    {
                        u.getParam(_a),
                        u.getParam(_d),
                        u.getParam(_s),
                        u.getParam(_r)
                    },
                    vals{ -1.f, -1.f, -1.f, -1.f },
                    img(juce::Image::ARGB, 1, 1, false)
                {
                }

                void update(bool forced = false) override
                {
                    const auto atk = params[A]->denormalized();
                    const auto dcy = params[D]->denormalized();
                    const auto sus = params[S]->getValue();
                    const auto rls = params[R]->denormalized();
                    if (forced || vals[A] != atk || vals[D] != dcy || vals[S] != sus || vals[R] != rls)
                    {
                        vals[A] = atk;
                        vals[D] = dcy;
                        vals[S] = sus;
                        vals[R] = rls;
                        updateImg();
                    }
                }

                void updateImg()
                {
                    const auto thicc = Shared::shared.thicc;
                    const auto bounds = getLocalBounds().toFloat().reduced(thicc);
                    const auto bY = bounds.getY();
                    const auto bW = bounds.getWidth();
                    const auto bH = bounds.getHeight();
                    //const auto bX = bounds.getX();

                    juce::Graphics g{ img };
                    const auto bgCol = Shared::shared.colour(ColourID::Bg);
                    g.fillAll(bgCol);

                    const auto col = Shared::shared.colour(ColourID::Interact);
                    g.setColour(col);

                    const auto atk = vals[A];
                    const auto dcy = vals[D];
                    const auto sus = vals[S];
                    const auto rls = vals[R];

                    vibrato::EnvGen envGen;

                    envGen.attack = atk;
                    envGen.decay = dcy;
                    envGen.sustain = sus;
                    envGen.release = rls;

                    envGen.prepare(100.f);

                    auto noteOn = true;
                    for (auto x = 0.f; x < bW; ++x)
                    {
                        const auto val = static_cast<float>(envGen(noteOn));
                        if(envGen.state == vibrato::EnvGen::State::D)
                            if (val - sus < .001f)
                                noteOn = false;
                        const auto hVal = bH * val;
                        const auto y = bY + bH - hVal;
                        g.fillRect(x, y, 1.f, hVal);
                    }
                    
                    repaint();
                }
            protected:
                std::array<Param*, 4> params;
                std::array<float, 4> vals;

                juce::Image img;

                void resized() override
                {
                    const auto thicc = Shared::shared.thicc;
                    const auto b = getLocalBounds().toFloat().reduced(thicc).toNearestInt();

                    img = juce::Image(juce::Image::ARGB, b.getWidth(), b.getHeight(), true);
                    update(true);
                }

                void paint(juce::Graphics& g) override
                {
                    const auto thicc = static_cast<int>(Shared::shared.thicc);

                    g.drawImageAt(img, thicc, thicc, false);
                }
            };

        public:
            ModCompAudioRate(Utils& u, std::vector<Paramtr*>& modulatables, int mOff = 0) :
                Comp(u, "", CursorType::Default),
                Timer(),
                layout(
                    { 30, 30, 30, 30, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 },
                    { 100, 100 }
                ),
                params {
                Paramtr(u, "Oct", "Transpose the oscillator in octave steps.", withOffset(PID::AudioRate0Oct, mOff), modulatables),
                Paramtr(u, "Semi", "Transpose the oscillator in semitone steps.", withOffset(PID::AudioRate0Semi, mOff), modulatables),
                Paramtr(u, "Fine", "Transpose the oscillator in finetone steps.", withOffset(PID::AudioRate0Fine, mOff), modulatables),
                Paramtr(u, "Width", "Defines this modulator's stereo-width.", withOffset(PID::AudioRate0Width, mOff), modulatables),
                Paramtr(u, "Legato", "Defines this oscillator's retune speed.", withOffset(PID::AudioRate0RetuneSpeed, mOff), modulatables),
                Paramtr(u, "A", "Defines the envelope's attack value.", withOffset(PID::AudioRate0Atk, mOff), modulatables),
                Paramtr(u, "D", "Defines the envelope's decay value.", withOffset(PID::AudioRate0Dcy, mOff), modulatables),
                Paramtr(u, "S", "Defines the envelope's sustain value.", withOffset(PID::AudioRate0Sus, mOff), modulatables),
                Paramtr(u, "R", "Defines the envelope's release value.", withOffset(PID::AudioRate0Rls, mOff), modulatables)
                },
                adsr(nullptr),
                wantsToReplaceADSR(false)
            {
                adsr = std::make_unique<ADSRRel>(u, params[Atk].getPID(), params[Dcy].getPID(), params[Sus].getPID(), params[Rls].getPID());

                adsr->onClick = [&w = wantsToReplaceADSR]()
                {
                    w = true;
                };

                for (auto& p: params)
                {
                    addAndMakeVisible(p);
                }
                
                addAndMakeVisible(*adsr);
                startTimerHz(24);
            }
            void activate(ParamtrRandomizer& randomizer)
            {
                for (auto& p : params)
                    randomizer.add(&p);
                setVisible(true);
            }
        protected:
            nelG::Layout layout;
            std::array<Paramtr, NumParams> params;
            std::unique_ptr<ADSRView> adsr;
            bool wantsToReplaceADSR;

            void timerCallback() override
            {
                adsr->update(false);
                if (wantsToReplaceADSR)
                {
                    removeChildComponent(adsr.get());
                    if (adsr->adsrViewType == 0)
                        adsr = std::make_unique<ADSRAbs>(utils, params[Atk].getPID(), params[Dcy].getPID(), params[Sus].getPID(), params[Rls].getPID());
                    else if (adsr->adsrViewType == 1)
                        adsr = std::make_unique<ADSRRel>(utils, params[Atk].getPID(), params[Dcy].getPID(), params[Sus].getPID(), params[Rls].getPID());
                    adsr->onClick = [&w = wantsToReplaceADSR]()
                    {
                        w = true;
                    };
                    layout.place(*adsr, 0, 0, 4, 1, 0.f, false);
                    addAndMakeVisible(*adsr);
                    wantsToReplaceADSR = false;
                }
            }

            void mouseEnter(const juce::MouseEvent& evt) override
            {
                Comp::mouseEnter(evt);
            }

            void paint(juce::Graphics&) override
            {
            }

            void resized() override
            {
                layout.setBounds(getLocalBounds().toFloat());

                layout.place(params[Atk],           0, 1, 1, 1, 0.f, true);
                layout.place(params[Dcy],           1, 1, 1, 1, 0.f, true);
                layout.place(params[Sus],           2, 1, 1, 1, 0.f, true);
                layout.place(params[Rls],           3, 1, 1, 1, 0.f, true);

                layout.place(*adsr,                  0, 0, 4, 1, 0.f, false);

                layout.place(params[Oct],           4, 0, 4, 1, 0.f, true);
                layout.place(params[Semi],          8, 0, 4, 1, 0.f, true);
                layout.place(params[Fine],          12, 0, 4, 1, 0.f, true);

                layout.place(params[Width],         4, 1, 6, 1, 0.f, true);
                layout.place(params[RetuneSpeed],   10, 1, 6, 1, 0.f, true);
            }
        };

        struct ModCompDropout :
            public Comp
        {
            enum { Decay, Spin, Chance, Smooth, Width, NumParams };

            ModCompDropout(Utils& u, std::vector<Paramtr*>& modulatables, int mOff = 0) :
                Comp(u, "", CursorType::Default),
                layout(
                    { 50, 50, 50, 50, 50, 30 },
                    { 20, 80 }
                ),
                params{
                    Paramtr(u, "Decay", "The approximate decay of the dropout.", withOffset(PID::Dropout0Decay, mOff), modulatables),
                    Paramtr(u, "Spin", "Give it a little spin.", withOffset(PID::Dropout0Spin, mOff), modulatables),
                    Paramtr(u, "Chance", "The likelyness of new dropouts to appear.", withOffset(PID::Dropout0Chance, mOff), modulatables),
                    Paramtr(u, "Hard", "Defines the smoothness of the dropouts.", withOffset(PID::Dropout0Smooth, mOff), modulatables),
                    Paramtr(u, "Width", "The modulator's stereo-width", withOffset(PID::Dropout0Width, mOff), modulatables)
            }
            {
                for (auto& p : params)
                {
                    addAndMakeVisible(p);
                }
            }
            void activate(ParamtrRandomizer& randomizer)
            {
                for (auto& p : params)
                    randomizer.add(&p);
                setVisible(true);
            }
        protected:
            nelG::Layout layout;
            std::array<Paramtr, NumParams> params;

            void mouseEnter(const juce::MouseEvent& evt) override
            {
                Comp::mouseEnter(evt);
            }

            void paint(juce::Graphics&) override
            {
            }

            void resized() override
            {
                layout.setBounds(getLocalBounds().toFloat());
                layout.place(params[Decay], 0, 1, 1, 1, 0.f, true);
                layout.place(params[Spin], 1, 0, 1, 2, 0.f, true);
                layout.place(params[Chance], 2, 1, 1, 1, 0.f, true);
                layout.place(params[Smooth], 3, 0, 1, 2, 0.f, true);
                layout.place(params[Width], 4, 1, 1, 1, 0.f, true);
            }
        };

        struct ModCompEnvFol :
            public Comp
        {
            enum { Attack, Release, Gain, Width, NumParams };

            ModCompEnvFol(Utils& u, std::vector<Paramtr*>& modulatables, int mOff = 0) :
                Comp(u, "", CursorType::Default),
                layout(
                    { 50, 50, 50, 50, 30 },
                    { 20, 80 }
                ),
                params{
                    Paramtr(u, "Attack", "The envelope follower's attack time in milliseconds.", withOffset(PID::EnvFol0Attack, mOff), modulatables),
                    Paramtr(u, "Release", "The envelope follower's release time in milliseconds.", withOffset(PID::EnvFol0Release, mOff), modulatables),
                    Paramtr(u, "Gain", "This modulator's input gain.", withOffset(PID::EnvFol0Gain, mOff), modulatables),
                    Paramtr(u, "Width", "The modulator's stereo-width", withOffset(PID::EnvFol0Width, mOff), modulatables)
            }
            {
                for (auto& p : params)
                {
                    addAndMakeVisible(p);
                }
            }
            void activate(ParamtrRandomizer& randomizer)
            {
                for (auto& p : params)
                    randomizer.add(&p);
                setVisible(true);
            }
        protected:
            nelG::Layout layout;
            std::array<Paramtr, NumParams> params;

            void mouseEnter(const juce::MouseEvent& evt) override
            {
                Comp::mouseEnter(evt);
            }

            void paint(juce::Graphics&) override
            {
            }

            void resized() override
            {
                layout.setBounds(getLocalBounds().toFloat());
                layout.place(params[Gain],   0, 1, 1, 1, 0.f, true);
                layout.place(params[Attack], 1, 0, 1, 2, 0.f, true);
                layout.place(params[Release],2, 0, 1, 2, 0.f, true);
                layout.place(params[Width],  3, 1, 1, 1, 0.f, true);
            }
        };

        struct ModCompMacro :
            public Comp
        {
            enum { Macro, NumParams };

            ModCompMacro(Utils& u, std::vector<Paramtr*>& modulatables, int mOff = 0) :
                Comp(u, "", CursorType::Default),
                layout(
                    { 100 },
                    { 100 }
                ),
                params{
                    Paramtr(u, "Macro", "Directly manipulate the vibrato's internal delay time", withOffset(PID::Macro0, mOff), modulatables)
            }
            {
                for (auto& p : params)
                {
                    addAndMakeVisible(p);
                }
            }
            void activate(ParamtrRandomizer& randomizer)
            {
                for (auto& p : params)
                    randomizer.add(&p);
                setVisible(true);
            }
        protected:
            nelG::Layout layout;
            std::array<Paramtr, NumParams> params;

            void mouseEnter(const juce::MouseEvent& evt) override
            {
                Comp::mouseEnter(evt);
            }

            void paint(juce::Graphics&) override
            {
            }

            void resized() override
            {
                layout.setBounds(getLocalBounds().toFloat());
                layout.place(params[Macro], 0, 0, 1, 1, 0.f, true);
            }
        };

        struct ModCompPitchbend :
            public Comp
        {
            ModCompPitchbend(Utils& u, std::vector<Paramtr*>& modulatables, int mOff = 0) :
                Comp(u, "Use your pitchbend-wheel to modulate the vibrato!", CursorType::Default),
                smooth(u, "Smooth", "Set the pitchbend-smoothing time in ms.", withOffset(PID::Pitchbend0Smooth, mOff), modulatables)
            {
                addAndMakeVisible(smooth);
                setInterceptsMouseClicks(false, true);
            }
        protected:
            Paramtr smooth;

            void paint(juce::Graphics& g) override
            {
                g.setColour(Shared::shared.colour(ColourID::Txt));
                g.drawFittedText(this->tooltip, getLocalBounds(), juce::Justification::centredBottom, 1);
            }
            void resized() override
            {
                smooth.setBounds(getLocalBounds().toFloat().reduced(Shared::shared.thicc * 4.f).toNearestInt());
            }
        };

        struct ModCompLFO :
            public Comp,
            public juce::Timer
        {
            using WTView = WavetableView<dsp::LFOTableSize, dsp::LFONumTables, 3, (1 << 7) + 1>;

            enum { IsSync, RateFree, RateSync, Waveform, Phase, Width, NumParams };

            ModCompLFO(Utils& u, std::vector<Paramtr*>& modulatables, dsp::LFOTables& _tables, int mOff = 0) :
                Comp(u, "", CursorType::Default),
                layout(
                    { 50, 50, 50, 50, 50 },
                    { 80, 40 }
                ),
                params
                {
                    Paramtr(u, "sync", "Switch between free running and temposync LFOs.", withOffset(PID::LFO0FreeSync, mOff), modulatables, ParameterType::Switch),
                    Paramtr(u, "Rate", "Adjust the frequency of the LFO in hz.", withOffset(PID::LFO0RateFree, mOff), modulatables),
                    Paramtr(u, "Rate", "Adjust the frequency of the LFO in beats.", withOffset(PID::LFO0RateSync, mOff), modulatables),
                    Paramtr(u, "WT", "Interpolate between the LFO's waveforms.", withOffset(PID::LFO0Waveform, mOff), modulatables),
                    Paramtr(u, "Phase", "Add a phase offset to the LFO.", withOffset(PID::LFO0Phase, mOff), modulatables),
                    Paramtr(u, "Width", "Add a phase offset to the right/side channel.", withOffset(PID::LFO0Width, mOff), modulatables)
                },
                lfoWaveformParam(*u.getParam(PID::LFO0Waveform, mOff)),
                tables(_tables),
                tableView(u, "Here you can admire this LFO's current waveform.", tables),
                wavetableBrowser(u),
                browserButton(u, "Click here to explore the wavetable browser."),
                btmText(""),
                isSync(-1)
            {
                addAndMakeVisible(tableView);
                addAndMakeVisible(browserButton);
                browserButton.onPaint = makeButtonOnPaintBrowse();
                browserButton.onClick = [this]()
                {
                    wavetableBrowser.setVisible(!wavetableBrowser.isVisible());
                };

                for (auto& p : params)
                    addChildComponent(p);
                params[Waveform].setVisible(true);
                params[Width].setVisible(true);
                params[Phase].setVisible(true);
                startTimerHz(4);
                initWavetableBrowser();
            }
            
            void activate(ParamtrRandomizer& randomizer)
            {
                for (auto& p : params)
                    randomizer.add(&p);
                randomizer.add([this](juce::Random& rand)
                    {
                        auto val = static_cast<int>(std::floor(rand.nextFloat() * 3.f));
                        switch (val)
                        {
                        case 0: tables.makeTablesSinc(); break;
                        case 1: tables.makeTablesTriangles(); break;
                        case 2: tables.makeTablesWeierstrasz(); break;
                        }
                        tableView.repaint();
                    }
                );
                setVisible(true);
            }
            
            Paramtr* getIsSyncButton() noexcept { return &params[IsSync]; }
            
        protected:
            nelG::Layout layout;
            std::array<Paramtr, NumParams> params;
            const Param& lfoWaveformParam;
            dsp::LFOTables& tables;
            WTView tableView;
            Browser wavetableBrowser;
            Button browserButton;
            juce::String btmText;
            int isSync;

            void mouseEnter(const juce::MouseEvent& evt) override
            {
                Comp::mouseEnter(evt);
            }

            void paint(juce::Graphics& g) override
            {
                g.setColour(Shared::shared.colour(ColourID::Txt));
                g.drawFittedText(btmText, getLocalBounds(), juce::Justification::bottomRight, 1);
            }

            void resized() override
            {
                layout.setBounds(getLocalBounds().toFloat());
                layout.place(params[IsSync],    0, 1, 1, 1, 0.f, true);
                layout.place(params[RateFree],  0, 0, 1, 1, 0.f, true);
                layout.place(params[RateSync],  0, 0, 1, 1, 0.f, true);
                layout.place(params[Waveform],  1, 0, 1, 1, 0.f, true);
                layout.place(params[Width],     2, 0, 1, 1, 0.f, true);
                layout.place(params[Phase],     3, 0, 1, 1, 0.f, true);

                layout.place(browserButton,     1, 1, 1, 1, 0.f, true);
                layout.place(tableView,         3, 0, 2, 2, 0.f, false);
                layout.place(wavetableBrowser,  2, 0, 3, 2, 0.f, false);
            }

            void timerCallback() override
            {
                tableView.update(lfoWaveformParam.getValueSum());

                if (this->utils.hasPlayHead())
                {
                    params[IsSync].setVisible(true);
                    const auto isSyncParam = this->utils.getParam(params[IsSync].getPID());
                    const auto _isSync = isSyncParam->getValueSum() < .5f ? 0 : 1;
                    if (isSync != _isSync)
                    {
                        isSync = _isSync;
                        params[RateSync].setVisible(isSync);
                        params[RateFree].setVisible(!isSync);
                    }
                    else
                    {
                        const auto phaseParam = utils.getParam(params[Phase].getPID());
                        if (phaseParam->getValSumDenorm() != 0.f)
                        {
                            if (btmText.isEmpty())
                                btmText = "<procedrl>";
                            repaint();
                        }
                        else
                        {
							if (!btmText.isEmpty())
								btmText = "";
                            repaint();
                        }
                    }
                }
                else
                {
                    params[IsSync].setVisible(false);
                    params[RateSync].setVisible(false);
                    params[RateFree].setVisible(true);
                    const auto isSyncParam = this->utils.getParam(params[IsSync].getPID());
                    if (isSyncParam->getValueSum() > .5f)
                        isSyncParam->setValueWithGesture(0.f);
                }
            }

        private:
            void initWavetableBrowser()
            {
                addChildComponent(wavetableBrowser);
                
                wavetableBrowser.addEntry(
                    "Weierstrasz",
                    "Modulate the vibrato with mesmerizing weierstrasz sinusoids.",
                    [this]()
                    {
                        tables.makeTablesWeierstrasz();
                        tableView.repaint();
                        wavetableBrowser.setVisible(false);
                    }
                );
                wavetableBrowser.addEntry(
                    "Triangles",
                    "Resample the signal with rich triangular textures.",
                    [this]()
                    {
                        tables.makeTablesTriangles();
                        tableView.repaint();
                        wavetableBrowser.setVisible(false);
                    }
                );
                wavetableBrowser.addEntry(
                    "Sinc",
                    "Everyone loves a good sinc function.",
                    [this]()
                    {
                        tables.makeTablesSinc();
                        tableView.repaint();
                        wavetableBrowser.setVisible(false);
                    }
                );
            }
        };

        class ModComp :
            public Comp,
            public juce::Timer
        {
            using ModType = vibrato::ModType;

            class Selector :
                public Comp
            {
                inline std::function<void()> makeOnClick(ModComp& modComp, vibrato::ModType modType)
                {
                    return [&comp = modComp, type = modType]()
                    {
                        comp.setMod(type);
                    };
                }
            public:
                Selector(Utils& u, ModComp& modComp) :
                    Comp(u, ""),
                    layout(
                        { 1, 1, 1, 1 },
                        { 1, 1 }
                    ),
                    perlin(u, "The perlin noise modulator uses natural noise to modulate the vibrato."),
                    audioRate(u, "The audio rate modulator uses a midi-note-controlled oscillator to modulate the vibrato."),
                    dropout(u, "The dropout modulator simulates random pitch dropouts similiar to tape artefacts."),
                    envFol(u, "The envelope follower modulates the vibrato according to your input signal's energy."),
                    macro(u, "Directly manipulate the vibrato's internal delay with this modulator."),
                    pitchbend(u, "Use your pitchbend wheel to modulate the vibrato with this modulator."),
                    lfo(u, "Modulate the vibrato with (more or less) classic LFO shapes.")
                {
                    perlin.onPaint = makeTextButtonOnPaint("Perlin");
                    audioRate.onPaint = makeTextButtonOnPaint("Audio\nRate");
                    dropout.onPaint = makeTextButtonOnPaint("Dropout");
                    envFol.onPaint = makeTextButtonOnPaint("Env\nFol");
                    macro.onPaint = makeTextButtonOnPaint("Macro");
                    pitchbend.onPaint = makeTextButtonOnPaint("Pitch\nBend");
                    lfo.onPaint = makeTextButtonOnPaint("LFO");

                    perlin.onClick = makeOnClick(modComp, vibrato::ModType::Perlin);
                    audioRate.onClick = makeOnClick(modComp, vibrato::ModType::AudioRate);
                    dropout.onClick = makeOnClick(modComp, vibrato::ModType::Dropout);
                    envFol.onClick = makeOnClick(modComp, vibrato::ModType::EnvFol);
                    macro.onClick = makeOnClick(modComp, vibrato::ModType::Macro);
                    pitchbend.onClick = makeOnClick(modComp, vibrato::ModType::Pitchwheel);
                    lfo.onClick = makeOnClick(modComp, vibrato::ModType::LFO);

                    addAndMakeVisible(perlin);
                    addAndMakeVisible(audioRate);
                    addAndMakeVisible(dropout);
                    addAndMakeVisible(envFol);
                    addAndMakeVisible(macro);
                    addAndMakeVisible(pitchbend);
                    addAndMakeVisible(lfo);
                }
            protected:
                nelG::Layout layout;
                Button perlin, audioRate, dropout, envFol, macro, pitchbend, lfo;

                void paint(juce::Graphics& g) override
                {
                    const auto thicc = Shared::shared.thicc;
                    const auto bounds = getLocalBounds().toFloat().reduced(thicc);
                    g.setColour(Shared::shared.colour(ColourID::Darken));
                    g.fillRoundedRectangle(bounds, thicc);
                }

                void resized() override
                {
                    const auto thicc = Shared::shared.thicc;
                    const auto bounds = getLocalBounds().toFloat();
                    layout.setBounds(bounds);
                    
                    layout.place(perlin,    0, 0, 1, 1, thicc, false);
                    layout.place(audioRate, 1, 0, 1, 1, thicc, false);
                    layout.place(dropout,   2, 0, 1, 1, thicc, false);
                    layout.place(envFol,    3, 0, 1, 1, thicc, false);
                    layout.place(macro,     0, 1, 1, 1, thicc, false);
                    layout.place(pitchbend, 1, 1, 1, 1, thicc, false);
                    layout.place(lfo,       2, 1, 1, 1, thicc, false);
                }
            };

            inline Notify makeNotify(ModComp& modComp)
            {
                return [&m = modComp](int type, const void*)
                {
                    if (type == NotificationType::PatchUpdated)
                    {
                        using namespace std::chrono_literals;
                        std::this_thread::sleep_for(100ms);
                        m.updateMod();
                    }
                    return false;
                };
            }

        public:
            ModComp(Utils& u, std::vector<Paramtr*>& modulatables,
                dsp::LFOTables& _tables, int _mOff = 0) :
                Comp(u, makeNotify(*this), "", CursorType::Default),
                layout(
                    { 80, 10, 10 },
                    { 30, 70 }
                ),
                modType(ModType::NumMods),
                modDepth(0.f),
                mOff(_mOff),

                onModChange([](ModType){}),
                label(u, "", ColourID::Transp, ColourID::Transp, ColourID::Mod),
                inputLabel(u, "", ColourID::Transp, ColourID::Transp, ColourID::Hover),

                perlin(u, modulatables, mOff),
                audioRate(u, modulatables, mOff),
                dropout(u, modulatables, mOff),
                envFol(u, modulatables, mOff),
                macro(u, modulatables, mOff),
                pitchbend(u, modulatables, mOff),
                lfo(u, modulatables, _tables, mOff),

                randomizer(u, "ModRandomizer" + juce::String(mOff)),
                selectorButton(u, "Select another modulator for this slot."),

                selector(nullptr)
            {
                label.font = Shared::shared.font;
                selectorButton.onPaint = makeTextButtonOnPaint("<<");
                selectorButton.onClick = [this]()
                {
                    if (selector == nullptr)
                    {
                        selector = std::make_unique<Selector>(this->utils, *this);
                        layout.place(*selector, 0, 1, 3, 1, 0.f, false);
                        addAndMakeVisible(*selector);
                        notify(NotificationType::KillPopUp);
                    }
                    else
                    {
                        selector.reset(nullptr);
                    }
                };

                addAndMakeVisible(label);
                addAndMakeVisible(inputLabel);
                inputLabel.setJustifaction(juce::Justification::left);
                
                addChildComponent(perlin);
                addChildComponent(audioRate);
                addChildComponent(dropout);
                addChildComponent(envFol);
                addChildComponent(macro);
                addChildComponent(pitchbend);
                addChildComponent(lfo);

                addAndMakeVisible(randomizer);
                addAndMakeVisible(selectorButton);
                
                setBufferedToImage(false);

                startTimerHz(24);
            }
            
            void updateMod()
            {
                setMod(getModType());
            }
            
            void setMod(ModType t)
            {
                if (modType == t)
                    return;
                
                modType = t;
                perlin.setVisible(false);
                audioRate.setVisible(false);
                dropout.setVisible(false);
                envFol.setVisible(false);
                macro.setVisible(false);
                pitchbend.setVisible(false);
                lfo.setVisible(false);
                randomizer.clear();
                inputLabel.setText("");

                switch (t)
                {
                case ModType::Perlin:
                    perlin.activate(randomizer);
                    label.setText("Perlin");
                    break;
                case ModType::AudioRate:
                    audioRate.activate(randomizer);
                    label.setText("AudioRate");
                    inputLabel.setText("midi in >>");
                    break;
                case ModType::Dropout:
                    dropout.activate(randomizer);
                    label.setText("Dropout");
                    break;
                case ModType::EnvFol:
                    envFol.activate(randomizer);
                    label.setText("Envelope\nFollower");
                    inputLabel.setText("audio in >>");
                    break;
                case ModType::Macro:
                    macro.activate(randomizer);
                    label.setText("Macro");
                    break;
                case ModType::Pitchwheel:
                    pitchbend.setVisible(true);
                    label.setText("Pitchbend");
                    inputLabel.setText("midi in >>");
                    break;
                case ModType::LFO:
                    lfo.activate(randomizer);
                    label.setText("LFO");
                    break;
                }

                label.repaint();
                inputLabel.repaint();
                selector.reset(nullptr);

                onModChange(t);
            }
            
            void randomizeAll()
            {
                randomizer(false);
            }

            std::function<void(ModType)> onModChange;
            
            void addButtonsToRandomizer(ParamtrRandomizer& randomizr)
            {
                randomizr.add(lfo.getIsSyncButton());
            }

            std::function<ModType()> getModType;
        protected:
            nelG::Layout layout;
            ModType modType;
            Label label, inputLabel;
            float modDepth;
            int mOff;

            ModCompPerlin perlin;
            ModCompAudioRate audioRate;
            ModCompDropout dropout;
            ModCompEnvFol envFol;
            ModCompMacro macro;
            ModCompPitchbend pitchbend;
            ModCompLFO lfo;

            ParamtrRandomizer randomizer;
            Button selectorButton;

            std::unique_ptr<Selector> selector;

            void paint(juce::Graphics& g) override
            {
                const auto thicc = Shared::shared.thicc;
                const auto col = Shared::shared.colour(modSys6::gui::ColourID::Hover).interpolatedWith(Shared::shared.colour(modSys6::gui::ColourID::Mod), modDepth);
                visualizeGroup(
                    g,
                    "Mod " + juce::String(mOff == 0 ? 0 : 1),
                    getLocalBounds().toFloat().reduced(thicc),
                    col,
                    thicc
                );
            }

            void resized() override
            {
                const auto thicc = Shared::shared.thicc;
                const auto thicc4 = thicc * 4.f;

                layout.setBounds(getLocalBounds().toFloat().reduced(thicc4));

                layout.place(label, 0, 0, 1, 1, thicc4, false);
                layout.place(inputLabel, 0, 0, 1, 1, thicc4, false);
                layout.place(randomizer, 1, 0, 1, 1, thicc, true);
                layout.place(selectorButton, 2, 0, 1, 1, thicc, true);

                {
                    const auto bounds = layout(0, 1, 3, 1, thicc, false).toNearestInt();
                    perlin.setBounds(bounds);
                    audioRate.setBounds(bounds);
                    dropout.setBounds(bounds);
                    envFol.setBounds(bounds);
                    macro.setBounds(bounds);
                    pitchbend.setBounds(bounds);
                    lfo.setBounds(bounds);
                }
            }

            void mouseEnter(const juce::MouseEvent& evt) override
            {
                Comp::mouseEnter(evt);
            }

            void timerCallback() override
            {
                setMod(getModType());

                const auto& param = *utils.getParam(PID::ModsMix);
                const auto valSum = param.getValueSum();
                const auto v = mOff == 0 ? 1.f - valSum : valSum;
                if (modDepth == v)
                    return;
                modDepth = v;
                repaint();
            }
        };
    }
}

/*

seed

*/