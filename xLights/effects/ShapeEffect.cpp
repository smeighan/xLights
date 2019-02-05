#include "ShapeEffect.h"
#include "ShapePanel.h"

#include "../sequencer/Effect.h"
#include "../RenderBuffer.h"
#include "../UtilClasses.h"
#include "../models/Model.h"
#include "../sequencer/SequenceElements.h"
#include "UtilFunctions.h"
#include "AudioManager.h"

#include "../../include/shape-16.xpm"
#include "../../include/shape-24.xpm"
#include "../../include/shape-32.xpm"
#include "../../include/shape-48.xpm"
#include "../../include/shape-64.xpm"

#define REPEATTRIGGER 20

ShapeEffect::ShapeEffect(int id) : RenderableEffect(id, "Shape", shape_16, shape_24, shape_32, shape_48, shape_64)
{
    //ctor
}

ShapeEffect::~ShapeEffect()
{
    //dtor
}

std::list<std::string> ShapeEffect::CheckEffectSettings(const SettingsMap& settings, AudioManager* media, Model* model, Effect* eff)
{
    std::list<std::string> res;

    if (media == nullptr && settings.GetBool("E_CHECKBOX_Shape_UseMusic", false))
    {
        res.push_back(wxString::Format("    WARN: Shape effect cant grow to music if there is no music. Model '%s', Start %s", model->GetName(), FORMATTIME(eff->GetStartTimeMS())).ToStdString());
    }

    return res;
}

void ShapeEffect::RenameTimingTrack(std::string oldname, std::string newname, Effect* effect)
{
    wxString timing = effect->GetSettings().Get("E_CHOICE_Shape_FireTimingTrack", "");

    if (timing.ToStdString() == oldname)
    {
        effect->GetSettings()["E_CHOICE_Shape_FireTimingTrack"] = wxString(newname);
    }

    SetPanelTimingTracks();
}

void ShapeEffect::SetPanelStatus(Model *cls)
{
    SetPanelTimingTracks();
}

void ShapeEffect::SetPanelTimingTracks() const
{
    ShapePanel *fp = (ShapePanel*)panel;
    if (fp == nullptr)
    {
        return;
    }

    if (mSequenceElements == nullptr)
    {
        return;
    }

    // Load the names of the timing tracks
    std::string timingtracks = "";
    for (size_t i = 0; i < mSequenceElements->GetElementCount(); i++)
    {
        Element* e = mSequenceElements->GetElement(i);
        if (e->GetEffectLayerCount() == 1 && e->GetType() == ELEMENT_TYPE_TIMING)
        {
            if (timingtracks != "") timingtracks += "|";
            timingtracks += e->GetName();
        }
    }

    wxCommandEvent event(EVT_SETTIMINGTRACKS);
    event.SetString(timingtracks);
    wxPostEvent(fp, event);
}

wxPanel *ShapeEffect::CreatePanel(wxWindow *parent) {
    return new ShapePanel(parent);
}

#define RENDER_SHAPE_CIRCLE     0
#define RENDER_SHAPE_SQUARE     1
#define RENDER_SHAPE_TRIANGLE   2
#define RENDER_SHAPE_STAR       3
#define RENDER_SHAPE_PENTAGON   4
#define RENDER_SHAPE_HEXAGON    5
#define RENDER_SHAPE_OCTAGON    6
#define RENDER_SHAPE_HEART      7
#define RENDER_SHAPE_TREE       8
#define RENDER_SHAPE_CANDYCANE  9
#define RENDER_SHAPE_SNOWFLAKE  10
#define RENDER_SHAPE_CRUCIFIX   11
#define RENDER_SHAPE_PRESENT    12
#define RENDER_SHAPE_EMOJI      13

void ShapeEffect::SetDefaultParameters() {
    ShapePanel *sp = (ShapePanel*)panel;
    if (sp == nullptr) {
        return;
    }

    sp->BitmapButton_Shape_ThicknessVC->SetActive(false);
    sp->BitmapButton_Shape_CentreXVC->SetActive(false);
    sp->BitmapButton_Shape_CentreYVC->SetActive(false);
    sp->BitmapButton_Shape_LifetimeVC->SetActive(false);
    sp->BitmapButton_Shape_GrowthVC->SetActive(false);
    sp->BitmapButton_Shape_CountVC->SetActive(false);
    sp->BitmapButton_Shape_StartSizeVC->SetActive(false);
    sp->BitmapButton_Shape_RotationVC->SetActive(false);

    SetChoiceValue(sp->Choice_Shape_ObjectToDraw, "Circle");

    SetSliderValue(sp->Slider_Shape_Thickness, 1);
    SetSliderValue(sp->Slider_Shape_StartSize, 1);
    SetSliderValue(sp->Slider_Shape_CentreX, 50);
    SetSliderValue(sp->Slider_Shape_CentreY, 50);
    SetSliderValue(sp->Slider_Shape_Points, 5);
    SetSliderValue(sp->Slider_Shape_Count, 5);
    SetSliderValue(sp->Slider_Shape_Growth, 10);
    SetSliderValue(sp->Slider_Shape_Lifetime, 5);
    SetSliderValue(sp->Slider_Shape_Sensitivity, 50);
    SetSliderValue(sp->Slider_Shape_Rotation, 0);

    SetCheckBoxValue(sp->CheckBox_Shape_RandomLocation, true);
    SetCheckBoxValue(sp->CheckBox_Shape_FadeAway, true);
    SetCheckBoxValue(sp->CheckBox_Shape_UseMusic, false);
    SetCheckBoxValue(sp->CheckBox_Shape_FireTiming, false);
    SetCheckBoxValue(sp->CheckBox_Shape_RandomInitial, true);
}

struct ShapeData
{
    wxPoint _centre;
    wxSize _movement;
    float _size;
    int _oset;
    xlColor _color;
    int _shape;
    float _angle;
    int _speed;

    ShapeData(wxPoint centre, float size, int oset, xlColor color, int shape, int angle, int speed)
    {
        _centre = centre;
        _movement.x = 0;
        _movement.y = 0;
        _size = size;
        _oset = oset;
        _color = color;
        _shape = shape;
        _angle = toRadians(angle);
        _speed = speed;
    }

    void Move()
    {
        int x = _speed * cos(_angle);
        int y = _speed * sin(_angle);
        _movement.x += x;
        _movement.y += y;
        _centre.x += x;
        _centre.y += y;
    }

    void SetCentre(wxPoint centre)
    {
        _centre = centre;
        _centre.x += _movement.x;
        _centre.y += _movement.y;
    }
};

bool compare_shapes(const ShapeData* first, const ShapeData* second)
{
    return first->_oset > second->_oset;
}

class ShapeRenderCache : public EffectRenderCache {

public:
    ShapeRenderCache() { _lastColorIdx = -1; _sinceLastTriggered = 0; }
    virtual ~ShapeRenderCache()
    {
        DeleteShapes();
    }

    std::list<ShapeData*> _shapes;
    int _lastColorIdx;
    int _sinceLastTriggered;
    wxFontInfo _font;

    void AddShape(wxPoint centre, float size, xlColor color, int oset, int shape, int angle, int speed, bool randomMovement)
    {
        if (randomMovement)
        {
            speed = rand01() * (SHAPE_VELOCITY_MAX - SHAPE_VELOCITY_MIN) - SHAPE_VELOCITY_MIN;
            angle = rand01() * (SHAPE_DIRECTION_MAX - SHAPE_DIRECTION_MIN) - SHAPE_VELOCITY_MIN;
        }
        _shapes.push_back(new ShapeData(centre, size, oset, color, shape, angle, speed));
    }

    void DeleteShapes()
    {
        while (_shapes.size() > 0)
        {
            auto todelete = _shapes.front();
            _shapes.pop_front();
            delete todelete;
        }
    }
    void RemoveOld(int maxAge)
    {
        // old are always at the front of the list
        while (_shapes.size() > 0 && _shapes.front()->_oset >= maxAge)
        {
            auto todelete = _shapes.front();
            _shapes.pop_front();
            delete todelete;
        }
    }
    void SortShapes()
    {
        _shapes.sort(compare_shapes);
    }
};

int ShapeEffect::DecodeShape(const std::string& shape)
{
    if (shape == "Circle")
    {
        return RENDER_SHAPE_CIRCLE;
    }
    else if (shape == "Square")
    {
        return RENDER_SHAPE_SQUARE;
    }
    else if (shape == "Triangle")
    {
        return RENDER_SHAPE_TRIANGLE;
    }
    else if (shape == "Star")
    {
        return RENDER_SHAPE_STAR;
    }
    else if (shape == "Pentagon")
    {
        return RENDER_SHAPE_PENTAGON;
    }
    else if (shape == "Hexagon")
    {
        return RENDER_SHAPE_HEXAGON;
    }
    else if (shape == "Heart")
    {
        return RENDER_SHAPE_HEART;
    }
    else if (shape == "Tree")
    {
        return RENDER_SHAPE_TREE;
    }
    else if (shape == "Octagon")
    {
        return RENDER_SHAPE_OCTAGON;
    }
    else if (shape == "Candy Cane")
    {
        return RENDER_SHAPE_CANDYCANE;
    }
    else if (shape == "Snowflake")
    {
        return RENDER_SHAPE_SNOWFLAKE;
    }
    else if (shape == "Crucifix")
    {
        return RENDER_SHAPE_CRUCIFIX;
    }
    else if (shape == "Present")
    {
        return RENDER_SHAPE_PRESENT;
    }
    // this must be the last as we dont want to randomly select it
    else if (shape == "Emoji")
    {
        return RENDER_SHAPE_EMOJI;
    }

    return rand01() * 13; // exclude emoji
}

void ShapeEffect::Render(Effect *effect, SettingsMap &SettingsMap, RenderBuffer &buffer) {

	float oset = buffer.GetEffectTimeIntervalPosition();

	std::string Object_To_DrawStr = SettingsMap["CHOICE_Shape_ObjectToDraw"];
    int thickness = GetValueCurveInt("Shape_Thickness", 1, SettingsMap, oset, SHAPE_THICKNESS_MIN, SHAPE_THICKNESS_MAX, buffer.GetStartTimeMS(), buffer.GetEndTimeMS());
    int points = SettingsMap.GetInt("SLIDER_Shape_Points", 5);
    bool randomLocation = SettingsMap.GetBool("CHECKBOX_Shape_RandomLocation", true);
    bool fadeAway = SettingsMap.GetBool("CHECKBOX_Shape_FadeAway", true);
    bool startRandomly = SettingsMap.GetBool("CHECKBOX_Shape_RandomInitial", true);
    int xc = GetValueCurveInt("Shape_CentreX", 50, SettingsMap, oset, SHAPE_CENTREX_MIN, SHAPE_CENTREX_MAX, buffer.GetStartTimeMS(), buffer.GetEndTimeMS()) * buffer.BufferWi / 100;
    int yc = GetValueCurveInt("Shape_CentreY", 50, SettingsMap, oset, SHAPE_CENTREY_MIN, SHAPE_CENTREY_MAX, buffer.GetStartTimeMS(), buffer.GetEndTimeMS()) * buffer.BufferHt / 100;
    int lifetime = GetValueCurveInt("Shape_Lifetime", 5, SettingsMap, oset, SHAPE_LIFETIME_MIN, SHAPE_LIFETIME_MAX, buffer.GetStartTimeMS(), buffer.GetEndTimeMS());
    int growth = GetValueCurveInt("Shape_Growth", 10, SettingsMap, oset, SHAPE_GROWTH_MIN, SHAPE_GROWTH_MAX, buffer.GetStartTimeMS(), buffer.GetEndTimeMS());
    int count = GetValueCurveInt("Shape_Count", 5, SettingsMap, oset, SHAPE_COUNT_MIN, SHAPE_COUNT_MAX, buffer.GetStartTimeMS(), buffer.GetEndTimeMS());
    int startSize = GetValueCurveInt("Shape_StartSize", 5, SettingsMap, oset, SHAPE_STARTSIZE_MIN, SHAPE_STARTSIZE_MAX, buffer.GetStartTimeMS(), buffer.GetEndTimeMS());
    int emoji = SettingsMap.GetInt("SPINCTRL_Shape_Char", 65);
    std::string font = SettingsMap["FONTPICKER_Shape_Font"];
    int direction = GetValueCurveInt("Shapes_Direction", 90, SettingsMap, oset, SHAPE_DIRECTION_MIN, SHAPE_DIRECTION_MAX, buffer.GetStartTimeMS(), buffer.GetEndTimeMS());
    int velocity = GetValueCurveInt("Shapes_Velocity", 0, SettingsMap, oset, SHAPE_VELOCITY_MIN, SHAPE_VELOCITY_MAX, buffer.GetStartTimeMS(), buffer.GetEndTimeMS());
    bool randomMovement = SettingsMap.GetBool("CHECKBOX_Shapes_RandomMovement", false);

    int rotation = GetValueCurveInt("Shape_Rotation", 0, SettingsMap, oset, SHAPE_ROTATION_MIN, SHAPE_ROTATION_MAX, buffer.GetStartTimeMS(), buffer.GetEndTimeMS());

    int Object_To_Draw = DecodeShape(Object_To_DrawStr);

    float f = 0.0;
    bool useMusic = SettingsMap.GetBool("CHECKBOX_Shape_UseMusic", false);
    float sensitivity = (float)SettingsMap.GetInt("SLIDER_Shape_Sensitivity", 50) / 100.0;
    bool useTiming = SettingsMap.GetBool("CHECKBOX_Shape_FireTiming", false);
    wxString timing = SettingsMap.Get("CHOICE_Shape_FireTimingTrack", "");
    if (timing == "") useTiming = false;
    if (useMusic)
    {
        if (buffer.GetMedia() != nullptr) {
            std::list<float>* pf = buffer.GetMedia()->GetFrameData(buffer.curPeriod, FRAMEDATA_HIGH, "");
            if (pf != nullptr)
            {
                f = *pf->begin();
            }
        }
    }

    ShapeRenderCache *cache = (ShapeRenderCache*)buffer.infoCache[id];
    if (cache == nullptr) {
        cache = new ShapeRenderCache();
        buffer.infoCache[id] = cache;
    }

    std::list<ShapeData*>& _shapes = cache->_shapes;
    int& _lastColorIdx = cache->_lastColorIdx;
    int& _sinceLastTriggered = cache->_sinceLastTriggered;
    wxFontInfo& _font = cache->_font;

    float lifetimeFrames = (float)(buffer.curEffEndPer - buffer.curEffStartPer) * lifetime / 100.0;
    if (lifetimeFrames < 1) lifetimeFrames = 1;
    float growthPerFrame = (float)growth / lifetimeFrames;

    if (buffer.needToInit)
    {
        buffer.needToInit = false;

        _sinceLastTriggered = 0;

        if (Object_To_Draw == RENDER_SHAPE_EMOJI)
        {
            wxFont ff(font);
            ff.SetNativeFontInfoUserDesc(font);

            _font = wxFontInfo(wxSize(0, 12));
            _font.FaceName(ff.GetFaceName());
            _font.Light();
            _font.AntiAliased(false);
            _font.Encoding(ff.GetEncoding());
        }

        cache->DeleteShapes();
        _lastColorIdx = -1;

        if (!useTiming && !useMusic)
        {
            for (int i = _shapes.size(); i < count; ++i)
            {
                wxPoint pt;
                if (randomLocation)
                {
                    pt = wxPoint(rand01() * buffer.BufferWi, rand01() * buffer.BufferHt);
                }
                else
                {
                    pt = wxPoint(xc, yc);
                }

                size_t colorcnt = buffer.GetColorCount();
                _lastColorIdx++;
                if (_lastColorIdx >= colorcnt)
                {
                    _lastColorIdx = 0;
                }

                int os = 0;
                if (startRandomly)
                {
                    os = rand01() * lifetimeFrames;
                }

                cache->AddShape(pt, startSize + os * growthPerFrame, buffer.palette.GetColor(_lastColorIdx), os, Object_To_Draw, direction, velocity, randomMovement);
            }
            cache->SortShapes();
        }
    }

    // create missing shapes
    if (useTiming)
    {
        if (mSequenceElements == nullptr)
        {
            // no timing tracks ... this shouldnt happen
        }
        else
        {
            // Load the names of the timing tracks
            Element* t = nullptr;
            for (size_t l = 0; l < mSequenceElements->GetElementCount(); l++)
            {
                Element* e = mSequenceElements->GetElement(l);
                if (e->GetEffectLayerCount() == 1 && e->GetType() == ELEMENT_TYPE_TIMING)
                {
                    if (e->GetName() == timing)
                    {
                        t = e;
                        break;
                    }
                }
            }

            if (t == nullptr)
            {
                // timing track not found ... this shouldnt happen
            }
            else
            {
                _sinceLastTriggered = 0;
                EffectLayer* el = t->GetEffectLayer(0);
                for (int j = 0; j < el->GetEffectCount(); j++)
                {
                    if (buffer.curPeriod == el->GetEffect(j)->GetStartTimeMS() / buffer.frameTimeInMs ||
                        buffer.curPeriod == el->GetEffect(j)->GetEndTimeMS() / buffer.frameTimeInMs)
                    {
                        wxPoint pt;
                        if (randomLocation)
                        {
                            pt = wxPoint(rand01() * buffer.BufferWi, rand01() * buffer.BufferHt);
                        }
                        else
                        {
                            pt = wxPoint(xc, yc);
                        }

                        size_t colorcnt = buffer.GetColorCount();
                        _lastColorIdx++;
                        if (_lastColorIdx >= colorcnt)
                        {
                            _lastColorIdx = 0;
                        }

                        cache->AddShape(pt, startSize, buffer.palette.GetColor(_lastColorIdx), 0, Object_To_Draw, direction, velocity, randomMovement);
                        break;
                    }
                }
            }
        }
    }
    else if (useMusic)
    {
        // only trigger a firework if music is greater than the sensitivity
        if (f > sensitivity)
        {
            // trigger if it was not previously triggered or has been triggered for REPEATTRIGGER frames
            if (_sinceLastTriggered == 0 || _sinceLastTriggered > REPEATTRIGGER)
            {
                wxPoint pt;
                if (randomLocation)
                {
                    pt = wxPoint(rand01() * buffer.BufferWi, rand01() * buffer.BufferHt);
                }
                else
                {
                    pt = wxPoint(xc, yc);
                }

                size_t colorcnt = buffer.GetColorCount();
                _lastColorIdx++;
                if (_lastColorIdx >= colorcnt)
                {
                    _lastColorIdx = 0;
                }

                cache->AddShape(pt, startSize, buffer.palette.GetColor(_lastColorIdx), 0, Object_To_Draw, direction, velocity, randomMovement);
            }

            // if music is over the trigger level for REPEATTRIGGER frames then we will trigger another firework
            _sinceLastTriggered++;
            if (_sinceLastTriggered > REPEATTRIGGER)
            {
                _sinceLastTriggered = 0;
            }
        }
        else
        {
            // not triggered so clear last triggered counter
            _sinceLastTriggered = 0;
        }
    }
    else
    {
        for (int i = _shapes.size(); i < count; ++i)
        {
            wxPoint pt;
            if (randomLocation)
            {
                pt = wxPoint(rand01() * buffer.BufferWi, rand01() * buffer.BufferHt);
            }
            else
            {
                pt = wxPoint(xc, yc);
            }

            size_t colorcnt = buffer.GetColorCount();
            _lastColorIdx++;
            if (_lastColorIdx >= colorcnt)
            {
                _lastColorIdx = 0;
            }

            cache->AddShape(pt, startSize, buffer.palette.GetColor(_lastColorIdx), 0, Object_To_Draw, direction, velocity, randomMovement);
        }
    }

    if (Object_To_Draw == RENDER_SHAPE_EMOJI)
    {
        auto context = buffer.GetTextDrawingContext();
        context->Clear();
    }

    for (auto& it : _shapes)
    {
        // if location is not random then update it to whatever the current location is
        // as it may be value curve controlled
        if (!randomLocation)
        {
            it->SetCentre(wxPoint(xc, yc));
        }

        it->Move();
        it->_oset++;
        it->_size += growthPerFrame;

        if (it->_size < 0) it->_size = 0;

        xlColor color = it->_color;

        if (fadeAway)
        {
            float brightness = (float)(lifetimeFrames - it->_oset) / lifetimeFrames;

            // draw text does not respect alpha
            if (buffer.allowAlpha && Object_To_Draw != RENDER_SHAPE_EMOJI) {
                color.alpha = 255.0 * brightness;
            }
            else {
                color.red = color.red * brightness;
                color.green = color.green * brightness;
                color.blue = color.blue * brightness;
            }
        }

        switch (it->_shape)
        {
        case RENDER_SHAPE_SQUARE:
            Drawpolygon(buffer, it->_centre.x, it->_centre.y, it->_size, 4, color, thickness, rotation + 45.0);
            break;
        case RENDER_SHAPE_CIRCLE:
            Drawcircle(buffer, it->_centre.x, it->_centre.y, it->_size, color, thickness);
            break;
        case RENDER_SHAPE_STAR:
            Drawstar(buffer, it->_centre.x, it->_centre.y, it->_size, points, color, thickness, rotation);
            break;
        case RENDER_SHAPE_TRIANGLE:
            Drawpolygon(buffer, it->_centre.x, it->_centre.y, it->_size, 3, color, thickness, rotation + 90.0);
            break;
        case RENDER_SHAPE_PENTAGON:
            Drawpolygon(buffer, it->_centre.x, it->_centre.y, it->_size, 5, color, thickness, rotation + 90.0);
            break;
        case RENDER_SHAPE_HEXAGON:
            Drawpolygon(buffer, it->_centre.x, it->_centre.y, it->_size, 6, color, thickness, rotation);
            break;
        case RENDER_SHAPE_OCTAGON:
            Drawpolygon(buffer, it->_centre.x, it->_centre.y, it->_size, 8, color, thickness, rotation + 22.5);
            break;
        case RENDER_SHAPE_TREE:
            Drawtree(buffer, it->_centre.x, it->_centre.y, it->_size, color, thickness);
            break;
        case RENDER_SHAPE_CRUCIFIX:
            Drawcrucifix(buffer, it->_centre.x, it->_centre.y, it->_size, color, thickness);
            break;
        case RENDER_SHAPE_PRESENT:
            Drawpresent(buffer, it->_centre.x, it->_centre.y, it->_size, color, thickness);
            break;
        case RENDER_SHAPE_EMOJI:
            Drawemoji(buffer, it->_centre.x, it->_centre.y, it->_size, color, emoji, _font);
            break;
        case RENDER_SHAPE_CANDYCANE:
            Drawcandycane(buffer, it->_centre.x, it->_centre.y, it->_size, color, thickness);
            break;
        case RENDER_SHAPE_SNOWFLAKE:
            Drawsnowflake(buffer, it->_centre.x, it->_centre.y, it->_size, 3, color, rotation + 30);
            break;
        case RENDER_SHAPE_HEART:
            Drawheart(buffer, it->_centre.x, it->_centre.y, it->_size, color, thickness);
            break;
        default:
            wxASSERT(false);
            break;
        }
    }

    if (Object_To_Draw == RENDER_SHAPE_EMOJI)
    {
        wxImage *i = buffer.GetTextDrawingContext()->FlushAndGetImage();
        unsigned char* data = i->GetData();
        unsigned char* alpha = i->HasAlpha() ? i->GetAlpha() : nullptr;
        int w = i->GetWidth();
        int h = i->GetHeight();
        int cur = 0;
        int curA = 0;
        for (int y = h - 1; y >= 0; y--)
        {
            for (int x = 0; x < w; x++)
            {
                xlColor c(data[cur], data[cur + 1], data[cur + 2]);
                if (alpha) {
                    c.SetAlpha(alpha[curA]);
                }
                if (c != xlBLACK) {
                    buffer.SetPixel(x, y, c);
                }
                cur += 3;
                ++curA;
            }
        }
    }

    cache->RemoveOld(lifetimeFrames);
}

void ShapeEffect::Drawcircle(RenderBuffer &buffer, int xc, int yc, double radius, xlColor color, int thickness) const
{
    double interpolation = 0.75;
    double t = (double)thickness - 1.0 + interpolation;
    for (double i = 0; i < t; i += interpolation)
    {
        if (radius >= 0)
        {
            for (double degrees = 0.0; degrees < 360.0; degrees += 1.0)
            {
                double radian = degrees * (M_PI / 180.0);
                int x = std::round(radius * buffer.cos(radian)) + xc;
                int y = std::round(radius * buffer.sin(radian)) + yc;
                buffer.SetPixel(x, y, color);
            }
        }
        else
        {
            break;
        }
        radius -= interpolation;
    }
}

void ShapeEffect::Drawstar(RenderBuffer &buffer, int xc, int yc, double radius, int points, xlColor color, int thickness, double rotation) const
{
    double interpolation = 0.6;
    double t = (double)thickness - 1.0 + interpolation;
    double offsetangle = 0.0;
    switch (points)
    {
    case 5:
        offsetangle = 90.0 - 360.0 / 5.0;
        break;
    case 6:
        offsetangle = 30.0;
        break;
    case 7:
        offsetangle = 90.0 - 360.0 / 7.0;
        break;
    default:
        break;
    }

    for (double i = 0; i < t; i += interpolation)
    {
        if (radius >= 0)
        {
            double InnerRadius = radius / 2.618034;    // divide by golden ratio squared

            double increment = 360.0 / points;

            for (double degrees = 0.0; degrees < 361.0; degrees += increment) // 361 because it allows for small rounding errors
            {
                if (degrees > 360.0) degrees = 360.0;
                double radian = (rotation + offsetangle + degrees) * (M_PI / 180.0);
                int xouter = std::round(radius * buffer.cos(radian)) + xc;
                int youter = std::round(radius * buffer.sin(radian)) + yc;

                radian = (rotation + offsetangle + degrees + increment / 2.0) * (M_PI / 180.0);
                int xinner = std::round(InnerRadius * buffer.cos(radian)) + xc;
                int yinner = std::round(InnerRadius * buffer.sin(radian)) + yc;

                buffer.DrawLine(xinner, yinner, xouter, youter, color);

                radian = (rotation + offsetangle + degrees - increment / 2.0) * (M_PI / 180.0);
                xinner = std::round(InnerRadius * buffer.cos(radian)) + xc;
                yinner = std::round(InnerRadius * buffer.sin(radian)) + yc;

                buffer.DrawLine(xinner, yinner, xouter, youter, color);

                if (degrees == 360.0) degrees = 361.0;
            }
        }
        else
        {
            break;
        }
        radius -= interpolation;
    }
}

void ShapeEffect::Drawpolygon(RenderBuffer &buffer, int xc, int yc, double radius, int sides, xlColor color, int thickness, double rotation) const
{
    double interpolation = 0.05;
    double t = (double)thickness - 1.0 + interpolation;
    double increment = 360.0 / sides;

    for (double i = 0; i < t; i += interpolation)
    {
        if (radius >= 0)
        {
            for (double degrees = 0.0; degrees < 361.0; degrees += increment) // 361 because it allows for small rounding errors
            {
                if (degrees > 360.0) degrees = 360.0;
                double radian = (rotation + degrees) * M_PI / 180.0;
                int x1 = std::round(radius * cos(radian)) + xc;
                int y1 = std::round(radius * sin(radian)) + yc;

                radian = (rotation + degrees + increment) * M_PI / 180.0;
                int x2 = std::round(radius * cos(radian)) + xc;
                int y2 = std::round(radius * sin(radian)) + yc;

                buffer.DrawLine(x1, y1, x2, y2, color);

                if (degrees == 360.0) degrees = 361.0;
            }
        }
        else
        {
            break;
        }
        radius -= interpolation;
    }
}

void ShapeEffect::Drawsnowflake(RenderBuffer &buffer, int xc, int yc, double radius, int sides, xlColor color, double rotation) const
{
    double increment = 360.0 / (sides * 2);
    double angle = rotation;

    if (radius >= 0)
    {
        for (int i = 0; i < sides * 2; i++)
        {
            double radian = angle * M_PI / 180.0;

            int x1 = std::round(radius * cos(radian)) + xc;
            int y1 = std::round(radius * sin(radian)) + yc;

            radian = (180 + angle) * M_PI / 180.0;

            int x2 = std::round(radius * cos(radian)) + xc;
            int y2 = std::round(radius * sin(radian)) + yc;

            buffer.DrawLine(x1, y1, x2, y2, color);

            angle += increment;
        }
    }
}

void ShapeEffect::Drawheart(RenderBuffer &buffer, int xc, int yc, double radius, xlColor color, int thickness) const
{
    double interpolation = 0.75;
    double t = (double)thickness - 1.0 + interpolation;

    for (double x = -2.0; x <= 2.0; x += 0.01f)
    {
        double y1 = std::sqrt(1.0 - (std::abs(x) - 1.0) * (std::abs(x) - 1.0));
        double y2 = std::acos(1.0 - std::abs(x)) - M_PI;

        double r = radius;

        for (double i = 0.0; i < t; i += interpolation)
        {
            if (r >= 0)
            {
                buffer.SetPixel(std::round(x * r / 2.0) + xc, std::round(y1 * r / 2.0) + yc, color);
                buffer.SetPixel(std::round(x * r / 2.0) + xc, std::round(y2 * r / 2.0) + yc, color);
            }
            else
            {
                break;
            }
            r -= interpolation;
        }
    }
}

void ShapeEffect::Drawtree(RenderBuffer &buffer, int xc, int yc, double radius, xlColor color, int thickness) const
{
    struct line
    {
        wxPoint start;
        wxPoint end;

        line(const wxPoint s, const wxPoint e)
        {
            start = s;
            end = e;
        }
    };

    const line points[] = {line(wxPoint(3,0), wxPoint(5,0)),
                           line(wxPoint(5,0), wxPoint(5,3)),
                           line(wxPoint(3,0), wxPoint(3,3)),
                           line(wxPoint(0,3), wxPoint(8,3)),
                           line(wxPoint(0,3), wxPoint(2,6)),
                           line(wxPoint(8,3), wxPoint(6,6)),
                           line(wxPoint(1,6), wxPoint(2,6)),
                           line(wxPoint(6,6), wxPoint(7,6)),
                           line(wxPoint(1,6), wxPoint(3,9)),
                           line(wxPoint(7,6), wxPoint(5,9)),
                           line(wxPoint(2,9), wxPoint(3,9)),
                           line(wxPoint(5,9), wxPoint(6,9)),
                           line(wxPoint(6,9), wxPoint(4,11)),
                           line(wxPoint(2,9), wxPoint(4,11))
    };
    int count = sizeof(points) / sizeof(line);

    double interpolation = 0.75;
    double t = (double)thickness - 1.0 + interpolation;

    for (double i = 0; i < t; i += interpolation)
    {
        if (radius >= 0)
        {
            for (int j = 0; j < count; ++j)
            {
                int x1 = std::round(((double)points[j].start.x - 4.0) / 11.0 * radius);
                int y1 = std::round(((double)points[j].start.y - 4.0) / 11.0 * radius);
                int x2 = std::round(((double)points[j].end.x - 4.0) / 11.0 * radius);
                int y2 = std::round(((double)points[j].end.y - 4.0) / 11.0 * radius);
                buffer.DrawLine(xc + x1, yc + y1, xc + x2, yc + y2, color);
            }
        }
        else
        {
            break;
        }
        radius -= interpolation;
    }
}

void ShapeEffect::Drawcrucifix(RenderBuffer &buffer, int xc, int yc, double radius, xlColor color, int thickness) const
{
    struct line
    {
        wxPoint start;
        wxPoint end;

        line(const wxPoint s, const wxPoint e)
        {
            start = s;
            end = e;
        }
    };

    const line points[] = {line(wxPoint(2,0), wxPoint(2,6)),
                           line(wxPoint(2,6), wxPoint(0,6)),
                           line(wxPoint(0,6), wxPoint(0,7)),
                           line(wxPoint(0,7), wxPoint(2,7)),
                           line(wxPoint(2,7), wxPoint(2,10)),
                           line(wxPoint(2,10), wxPoint(3,10)),
                           line(wxPoint(3,10), wxPoint(3,7)),
                           line(wxPoint(3,7), wxPoint(5,7)),
                           line(wxPoint(5,7), wxPoint(5,6)),
                           line(wxPoint(5,6), wxPoint(3,6)),
                           line(wxPoint(3,6), wxPoint(3,0)),
                           line(wxPoint(3,0), wxPoint(2,0))
    };
    int count = sizeof(points) / sizeof(line);

    double interpolation = 0.75;
    double t = (double)thickness - 1.0 + interpolation;

    for (double i = 0; i < t; i += interpolation)
    {
        if (radius >= 0)
        {
            for (int j = 0; j < count; ++j)
            {
                int x1 = std::round(((double)points[j].start.x - 2.5) / 7.0 * radius);
                int y1 = std::round(((double)points[j].start.y - 6.5) / 10.0 * radius);
                int x2 = std::round(((double)points[j].end.x - 2.5) / 7.0 * radius);
                int y2 = std::round(((double)points[j].end.y - 6.5) / 10.0 * radius);
                buffer.DrawLine(xc + x1, yc + y1, xc + x2, yc + y2, color);
            }
        }
        else
        {
            break;
        }
        radius -= interpolation;
    }
}

void ShapeEffect::Drawpresent(RenderBuffer &buffer, int xc, int yc, double radius, xlColor color, int thickness) const
{
    struct line
    {
        wxPoint start;
        wxPoint end;

        line(const wxPoint s, const wxPoint e)
        {
            start = s;
            end = e;
        }
    };

    const line points[] = {line(wxPoint(0,0), wxPoint(0,9)),
                           line(wxPoint(0,9), wxPoint(10,9)),
                           line(wxPoint(10,9), wxPoint(10,0)),
                           line(wxPoint(10,0), wxPoint(0,0)),
                           line(wxPoint(5,0), wxPoint(5,9)),
                           line(wxPoint(5,9), wxPoint(2,11)),
                           line(wxPoint(2,11), wxPoint(2,9)),
                           line(wxPoint(5,9), wxPoint(8,11)),
                           line(wxPoint(8,11), wxPoint(8,9))
    };
    int count = sizeof(points) / sizeof(line);

    double interpolation = 0.75;
    double t = (double)thickness - 1.0 + interpolation;

    for (double i = 0; i < t; i += interpolation)
    {
        if (radius >= 0)
        {
            for (int j = 0; j < count; ++j)
            {
                int x1 = std::round(((double)points[j].start.x - 5) / 7.0 * radius);
                int y1 = std::round(((double)points[j].start.y - 5.5) / 10.0 * radius);
                int x2 = std::round(((double)points[j].end.x - 5) / 7.0 * radius);
                int y2 = std::round(((double)points[j].end.y - 5.5) / 10.0 * radius);
                buffer.DrawLine(xc + x1, yc + y1, xc + x2, yc + y2, color);
            }
        }
        else
        {
            break;
        }
        radius -= interpolation;
    }
}

void ShapeEffect::Drawemoji(RenderBuffer& buffer, int xc, int yc, double radius, xlColor color, int emoji, wxFontInfo& font) const
{
    if (radius < 1) return;

    auto context = buffer.GetTextDrawingContext();

    wxFontInfo fi(wxSize(0, radius));
    fi.FaceName(font.GetFaceName());
    fi.Light(font.GetWeight() == wxFONTWEIGHT_LIGHT);
    fi.AntiAliased(font.IsAntiAliased());
    fi.Encoding(font.GetEncoding());
    
    context->SetFont(fi, color);

    wchar_t ch = emoji;

    auto text = wxString(ch);

    double width;
    double height;
    context->GetTextExtent(text, &width, &height);

    context->DrawText(text, std::round((float)xc - width / 2.0), std::round((float)yc - height / 2.0));
}

void ShapeEffect::Drawcandycane(RenderBuffer &buffer, int xc, int yc, double radius, xlColor color, int thickness) const
{
    double originalRadius = radius;
    double interpolation = 0.75;
    double t = (double)thickness - 1.0 + interpolation;
    for (double i = 0; i < t; i += interpolation)
    {
        if (radius >= 0)
        {
            // draw the stick
            int y1 = std::round((double)yc + originalRadius / 6.0);
            int y2 = std::round((double)yc - originalRadius / 2.0);
            int x = std::round((double)xc + radius / 2.0);
            buffer.DrawLine(x, y1, x, y2, color);

            // draw the hook
            double r = radius / 3.0;
            for (double degrees = 0.0; degrees < 180; degrees += 1.0)
            {
                double radian = degrees * (M_PI / 180.0);
                x = std::round((r-interpolation) * buffer.cos(radian) + xc + originalRadius / 6.0);
                int y = std::round((r - interpolation) * buffer.sin(radian) + y1);
                buffer.SetPixel(x, y, color);
            }
        }
        else
        {
            break;
        }
        radius -= interpolation;
    }
}
