/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define LUA_LIB

#include "lua_api.h"
#include "lua_widget.h"
#include "edgetx.h"

#include "api_colorlcd.h"

class LvglWidgetParams
{
 public:
  LvglWidgetParams(lua_State *L, int index = 1)
  {
    luaL_checktype(L, index, LUA_TTABLE);
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
      const char *key = lua_tostring(L, -2);
      if (!strcmp(key, "type")) {
        if (lua_isinteger(L, -1)) {
          int n = lua_tointeger(L, -1);
          if (n > ETX_UNDEF && n < ETX_LAST)
            type = (LuaLvglType)n;
          else
            type = ETX_UNDEF;
        } else {
          type = getType(luaL_checkstring(L, -1));
        }
      } else if (!strcmp(key, "name")) {
        name = luaL_checkstring(L, -1);
      } else if (!strcmp(key, "children")) {
        hasChildren = true;
      }
    }
  }

  LuaLvglType getType(const char* s)
  {
    if (strcasecmp(s, "label") == 0) return ETX_LABEL;
    if (strcasecmp(s, "rectangle") == 0) return ETX_RECTANGLE;
    if (strcasecmp(s, "circle") == 0) return ETX_CIRCLE;
    if (strcasecmp(s, "arc") == 0) return ETX_ARC;
    if (strcasecmp(s, "hline") == 0) return ETX_HLINE;
    if (strcasecmp(s, "vline") == 0) return ETX_VLINE;
    if (strcasecmp(s, "line") == 0) return ETX_LINE;
    if (strcasecmp(s, "triangle") == 0) return ETX_TRIANGLE;
    if (strcasecmp(s, "image") == 0) return ETX_IMAGE;
    if (strcasecmp(s, "qrcode") == 0) return ETX_QRCODE;
    if (strcasecmp(s, "box") == 0) return ETX_BOX;
    if (strcasecmp(s, "button") == 0) return ETX_BUTTON;
    if (strcasecmp(s, "momentaryButton") == 0) return ETX_MOMENTARY_BUTTON;
    if (strcasecmp(s, "toggle") == 0) return ETX_TOGGLE;
    if (strcasecmp(s, "textEdit") == 0) return ETX_TEXTEDIT;
    if (strcasecmp(s, "numberEdit") == 0) return ETX_NUMBEREDIT;
    if (strcasecmp(s, "choice") == 0) return ETX_CHOICE;
    if (strcasecmp(s, "slider") == 0) return ETX_SLIDER;
    if (strcasecmp(s, "verticalSlider") == 0) return ETX_VERTICAL_SLIDER;
    if (strcasecmp(s, "page") == 0) return ETX_PAGE;
    if (strcasecmp(s, "font") == 0) return ETX_FONT;
    if (strcasecmp(s, "align") == 0) return ETX_ALIGN;
    if (strcasecmp(s, "color") == 0) return ETX_COLOR;
    if (strcasecmp(s, "timer") == 0) return ETX_TIMER;
    if (strcasecmp(s, "switch") == 0) return ETX_SWITCH;
    if (strcasecmp(s, "source") == 0) return ETX_SOURCE;
    if (strcasecmp(s, "file") == 0) return ETX_FILE;
    if (strcasecmp(s, "setting") == 0) return ETX_SETTING;
    return ETX_UNDEF;
  }

  LuaLvglType type = ETX_UNDEF;
  const char *name = nullptr;
  bool hasChildren = false;
};

static int luaLvglPopup(lua_State *L, std::function<LvglWidgetObjectBase*()> create)
{
  auto obj = create();
  obj->create(L, 1);
  return 0;
}

static int luaDestroyLvglWidget(lua_State *L)
{
  auto p = LvglWidgetObjectBase::checkLvgl(L, 1);
  if (p) {
    p->clearRefs(L);
    delete p;
  }
  return 0;
}

/*luadoc
@function lvgl.set(obj, params)

Update an existing LVGL object from a parameter table.

@param obj (table) LVGL object previously created by the `lvgl` library

@param params (table) updated properties for the object

@status current Introduced in 3.0.0
*/
static int luaLvglSet(lua_State *L)
{
  auto p = LvglWidgetObjectBase::checkLvgl(L, 1, true);
  if (p) {
    p->update(L);
  }
  return 0;
}

/*luadoc
@function lvgl.clear([obj])

Clear all LVGL content created by the current script, or clear the children of a specific object.

@param obj (optional table) LVGL object to clear instead of clearing the whole script UI

@status current Introduced in 3.0.0
*/
static int luaLvglClear(lua_State *L)
{
  if (luaScriptManager) {
    if (lua_gettop(L) == 1) {
      auto p = LvglWidgetObjectBase::checkLvgl(L, 1, true);
      if (p) {
        p->clear();
      }
    } else {
      luaScriptManager->clear();
    }
  }

  return 0;
}

/*luadoc
@function lvgl.show(obj)

Show an LVGL object that was previously hidden.

@param obj (table) LVGL object previously created by the `lvgl` library

@status current Introduced in 3.0.0
*/
static int luaLvglShow(lua_State *L)
{
  auto p = LvglWidgetObjectBase::checkLvgl(L, 1, true);
  if (p) {
    p->show();
  }
  return 0;
}

/*luadoc
@function lvgl.hide(obj)

Hide an LVGL object without destroying it.

@param obj (table) LVGL object previously created by the `lvgl` library

@status current Introduced in 3.0.0
*/
static int luaLvglHide(lua_State *L)
{
  auto p = LvglWidgetObjectBase::checkLvgl(L, 1, true);
  if (p) {
    p->hide();
  }
  return 0;
}

/*luadoc
@function lvgl.enable(obj)

Enable interaction for an LVGL control object.

@param obj (table) LVGL object previously created by the `lvgl` library

@status current Introduced in 3.0.0
*/
static int luaLvglEnable(lua_State *L)
{
  auto p = LvglWidgetObjectBase::checkLvgl(L, 1, true);
  if (p) {
    p->enable();
  }
  return 0;
}

/*luadoc
@function lvgl.disable(obj)

Disable interaction for an LVGL control object.

@param obj (table) LVGL object previously created by the `lvgl` library

@status current Introduced in 3.0.0
*/
static int luaLvglDisable(lua_State *L)
{
  auto p = LvglWidgetObjectBase::checkLvgl(L, 1, true);
  if (p) {
    p->disable();
  }
  return 0;
}

/*luadoc
@function lvgl.close(obj)

Close an LVGL object that represents a closable container or dialog.

@param obj (table) LVGL object previously created by the `lvgl` library

@status current Introduced in 3.0.0
*/
static int luaLvglClose(lua_State *L)
{
  auto p = LvglWidgetObjectBase::checkLvgl(L, 1, true);
  if (p) {
    p->close();
  }
  return 0;
}

static void buildLvgl(lua_State *L, int srcIndex, int refIndex)
{
  luaL_checktype(L, srcIndex, LUA_TTABLE);
  for (lua_pushnil(L); lua_next(L, srcIndex - 1); lua_pop(L, 1)) {
    auto t = lua_gettop(L);
    LvglWidgetParams p(L, -1);
    if (p.type >= ETX_FIRST_CONTROL && !luaScriptManager->isFullscreen())
      continue;
    LvglWidgetObjectBase *obj = nullptr;
    switch (p.type) {
      case ETX_LABEL:
        obj = new LvglWidgetLabel();
        break;
      case ETX_RECTANGLE:
        obj = new LvglWidgetRectangle();
        break;
      case ETX_CIRCLE:
        obj = new LvglWidgetCircle();
        break;
      case ETX_ARC:
        obj = new LvglWidgetArc();
        break;
      case ETX_HLINE:
        obj = new LvglWidgetHLine();
        break;
      case ETX_VLINE:
        obj = new LvglWidgetVLine();
        break;
      case ETX_LINE:
        obj = new LvglWidgetLine();
        break;
      case ETX_TRIANGLE:
        obj = new LvglWidgetTriangle();
        break;
      case ETX_IMAGE:
        obj = new LvglWidgetImage();
        break;
      case ETX_QRCODE:
        obj = new LvglWidgetQRCode();
        break;
      case ETX_BOX:
        obj = new LvglWidgetBox();
        break;
      case ETX_BUTTON:
        obj = new LvglWidgetTextButton();
        break;
      case ETX_MOMENTARY_BUTTON:
        obj = new LvglWidgetMomentaryButton();
        break;
      case ETX_TOGGLE:
        obj = new LvglWidgetToggleSwitch();
        break;
      case ETX_TEXTEDIT:
        obj = new LvglWidgetTextEdit();
        break;
      case ETX_NUMBEREDIT:
        obj = new LvglWidgetNumberEdit();
        break;
      case ETX_CHOICE:
        obj = new LvglWidgetChoice();
        break;
      case ETX_SLIDER:
        obj = new LvglWidgetSlider();
        break;
      case ETX_VERTICAL_SLIDER:
        obj = new LvglWidgetVerticalSlider();
        break;
      case ETX_PAGE:
        obj = new LvglWidgetPage();
        break;
      case ETX_FONT:
        obj = new LvglWidgetFontPicker();
        break;
      case ETX_ALIGN:
        obj = new LvglWidgetAlignPicker();
        break;
      case ETX_COLOR:
        obj = new LvglWidgetColorPicker();
        break;
      case ETX_TIMER:
        obj = new LvglWidgetTimerPicker();
        break;
      case ETX_SWITCH:
        obj = new LvglWidgetSwitchPicker();
        break;
      case ETX_SOURCE:
        obj = new LvglWidgetSourcePicker();
        break;
      case ETX_FILE:
        obj = new LvglWidgetFilePicker();
        break;
      case ETX_SETTING:
        obj = new LvglWidgetSetting();
        break;
      default:
        continue;
    }
    if (obj) {
      obj->create(L, -1);
      auto ref = obj->getRef(L);
      if (p.name && refIndex != 0) {
        lua_pushstring(L, p.name);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        lua_settable(L, refIndex - 4);
      }
      if (p.hasChildren && obj->getWindow()) {
        lua_getfield(L, -1, "children");
        auto prevParent = luaScriptManager->getTempParent();
        luaScriptManager->setTempParent(obj);
        buildLvgl(L, -1, (refIndex != 0) ? refIndex - 3 : LUA_REFNIL);
        lua_pop(L, 1);
        luaScriptManager->setTempParent(prevParent);
      }
    }
    lua_settop(L, t); // In case of errors in build functions
  }
}

static void addChildren(lua_State *L, LvglWidgetObjectBase* obj)
{
  if (obj->getWindow()) {
    lua_getfield(L, -1, "children");
    auto prevParent = luaScriptManager->getTempParent();
    luaScriptManager->setTempParent(obj);
    buildLvgl(L, -1, 0);
    lua_pop(L, 1);
    luaScriptManager->setTempParent(prevParent);
  }
}

static int luaLvglObj(lua_State *L, std::function<LvglWidgetObject*()> create, bool fullscreenOnly = false)
{
  if (luaScriptManager && (!fullscreenOnly || luaScriptManager->isFullscreen())) {
    LvglWidgetParams params(L, 1);

    auto obj = create();
    obj->create(L, 1);

    if (params.hasChildren) addChildren(L, obj);

    obj->push(L);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

static int luaLvglObjEx(lua_State *L, std::function<LvglWidgetObjectBase*()> create, bool fullscreenOnly = false)
{
  if (luaScriptManager && (!fullscreenOnly || luaScriptManager->isFullscreen())) {
    LvglWidgetObjectBase* p = nullptr;
    LvglWidgetObjectBase* prevParent = nullptr;
    if (lua_gettop(L) == 2) {
      p = LvglWidgetObjectBase::checkLvgl(L, 1, true);
      if (p) {
        prevParent = luaScriptManager->getTempParent();
        luaScriptManager->setTempParent(p);
      }
    }

    LvglWidgetParams params(L, -1);

    auto obj = create();
    obj->create(L, -1);

    if (params.hasChildren) addChildren(L, obj);

    obj->push(L);

    if (p)
      luaScriptManager->setTempParent((prevParent));
  } else {
    lua_pushnil(L);
  }

  return 1;
}

/*luadoc
@function lvgl.build(definition)

Build multiple LVGL objects from a Lua table definition and return the created object references.

@param definition (table) array-like table of object definition tables

@retval table array of created LVGL objects

@notice Object definitions commonly use:
 * `type` (string or number) object type
 * `name` (string) optional name used in the returned reference table
 * `children` (table) nested child object definitions for containers

@notice Most objects also accept common keys such as:
 * `x`, `y`, `w`, `h`
 * `color`, `opacity`
 * `visible` returning a boolean
 * `pos` returning x and y
 * `size` returning width and height

@status current Introduced in 3.0.0
*/
static int luaLvglBuild(lua_State *L)
{
  if (luaScriptManager) {
    LvglWidgetObjectBase* p = nullptr;
    LvglWidgetObjectBase* prevParent = nullptr;
    if (lua_gettop(L) == 2) {
      p = LvglWidgetObjectBase::checkLvgl(L, 1, true);
      if (p) {
        prevParent = luaScriptManager->getTempParent();
        luaScriptManager->setTempParent(p);
      }
    }

    // Return array of lvgl object references
    lua_newtable(L);
    buildLvgl(L, -2, -1);

    if (p)
      luaScriptManager->setTempParent((prevParent));
  } else {
    lua_pushnil(L);
  }
  return 1;
}

/*luadoc
@function lvgl.isAppMode()

Return whether the current script is running in app mode.

@retval boolean true when the current script runs in app mode

@status current Introduced in 3.0.0
*/
static int luaLvglIsAppMode(lua_State *L)
{
  if (luaScriptManager) {
    lua_pushboolean(L, luaScriptManager->isAppMode());
  } else {
    lua_pushboolean(L, false);
  }
  return 1;
}

/*luadoc
@function lvgl.isFullScreen()

Return whether the current script is running in fullscreen mode.

@retval boolean true when the current script runs in fullscreen mode

@status current Introduced in 3.0.0
*/
static int luaLvglIsFullscreen(lua_State *L)
{
  if (luaScriptManager) {
    lua_pushboolean(L, luaScriptManager->isFullscreen());
  } else {
    lua_pushboolean(L, false);
  }
  return 1;
}

/*luadoc
@function lvgl.exitFullScreen()

Exit LVGL fullscreen mode for the current script.

@status current Introduced in 3.0.0
*/
static int luaLvglExitFullscreen(lua_State *L)
{
  if (luaScriptManager)
    luaScriptManager->exitFullscreen();
  return 0;
}

/*luadoc
@function lvgl.getContext()

Return the Lua context table associated with the current LVGL script, if any.

@retval table current LVGL context table

@retval nil no LVGL context is available

@status current Introduced in 3.0.0
*/
static int luaLvglGetContext(lua_State *L)
{
  if (luaScriptManager && luaScriptManager->getContext() != LUA_REFNIL) {
    // Push context tanle onto Lua stack (return object)
    lua_rawgeti(L, LUA_REGISTRYINDEX, luaScriptManager->getContext());
  } else {
    lua_pushnil(L);
  }
  return 1;
}

/*luadoc
@function lvgl.getScrollPos(obj)

Return the current scroll position of an LVGL object.

@param obj (table) LVGL object previously created by the `lvgl` library

@retval x,y (numbers) horizontal and vertical scroll offsets

@status current Introduced in 3.0.0
*/
static int luaLvglGetScrollPos(lua_State *L)
{
  auto p = LvglWidgetObjectBase::checkLvgl(L, 1, true);
  if (p) {
    lua_pushinteger(L, p->getScrollX());
    lua_pushinteger(L, p->getScrollY());
    return 2;
  }
  return 0;
}

/*luadoc
@function lvgl.label(params)

Create an LVGL label object.

@param params (table) object definition table

@retval table created LVGL object

@notice Common keys:
 * `x`, `y`, `w`, `h`
 * `color`, `opacity`
 * `visible`, `pos`, `size`

@notice Label keys:
 * `text` (string or function)
 * `font` (number or function)
 * `align` (number or function)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.rectangle(params)

Create an LVGL rectangle object.

@param params (table) object definition table

@retval table created LVGL object

@notice Common keys:
 * `x`, `y`, `w`, `h`
 * `color`, `opacity`, `visible`

@notice Rectangle keys:
 * `rounded` (number)
 * `thickness` (number)
 * `filled` (boolean or function)
 * `flexFlow`, `flexPad`, `borderPad`
 * `align`, `scrollBar`, `scrollDir`, `scrollTo`, `scrolled`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.hline(params)

Create an LVGL horizontal line object.

@param params (table) object definition table

@retval table created LVGL object

@notice Common keys:
 * `x`, `y`, `w`, `h`
 * `color`, `opacity`, `visible`

@notice Horizontal line keys:
 * `rounded` (boolean)
 * `dashGap` (number)
 * `dashWidth` (number)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.vline(params)

Create an LVGL vertical line object.

@param params (table) object definition table

@retval table created LVGL object

@notice Common keys:
 * `x`, `y`, `w`, `h`
 * `color`, `opacity`, `visible`

@notice Vertical line keys:
 * `rounded` (boolean)
 * `dashGap` (number)
 * `dashWidth` (number)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.line(params)

Create an LVGL line object.

@param params (table) object definition table

@retval table created LVGL object

@notice Line keys:
 * `color`, `opacity`, `visible`
 * `rounded` (boolean)
 * `thickness` (number)
 * `pts` (table of `{x, y}` points or function returning that table)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.triangle(params)

Create an LVGL triangle object.

@param params (table) object definition table

@retval table created LVGL object

@notice Triangle keys:
 * `color`, `opacity`, `visible`
 * `pts` (table with three `{x, y}` points or function returning that table)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.circle(params)

Create an LVGL circle object.

@param params (table) object definition table

@retval table created LVGL object

@notice Common bordered-object keys:
 * `color`, `opacity`, `visible`
 * `thickness` (number)
 * `filled` (boolean or function)
 * `radius` (number or function)
 * `flexFlow`, `flexPad`, `borderPad`

@notice Circle position uses the center point with `radius`.

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.arc(params)

Create an LVGL arc object.

@param params (table) object definition table

@retval table created LVGL object

@notice Arc keys:
 * `radius` (number or function)
 * `rounded` (boolean)
 * `thickness` (number)
 * `startAngle`, `endAngle`
 * `bgColor`, `bgOpacity`
 * `bgStartAngle`, `bgEndAngle`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.image(params)

Create an LVGL image object.

@param params (table) object definition table

@retval table created LVGL object

@notice Image keys:
 * `x`, `y`, `w`, `h`
 * `file` (string or function)
 * `fill` (boolean)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.qrcode(params)

Create an LVGL QR code object.

@param params (table) object definition table

@retval table created LVGL object

@notice QR code keys:
 * `x`, `y`, `w`
 * `data` (string)
 * `color` (number)
 * `bgColor` (number)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.button(params)

Create an LVGL text button control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Button keys:
 * `x`, `y`, `w`, `h`
 * `text`, `font`
 * `color`, `textColor`
 * `cornerRadius` (number)
 * `press` (function)
 * `checked` (boolean)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.momentaryButton(params)

Create an LVGL momentary button control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Momentary button keys:
 * `x`, `y`, `w`, `h`
 * `text`, `font`
 * `color`, `textColor`
 * `cornerRadius` (number)
 * `press` (function)
 * `release` (function)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.toggle(params)

Create an LVGL toggle control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Toggle keys:
 * `x`, `y`
 * `get` (function)
 * `set` (function)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.textEdit(params)

Create an LVGL text edit control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Text edit keys:
 * `x`, `y`, `w`, `h`
 * `value` (string)
 * `length` (number)
 * `set` (function)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.numberEdit(params)

Create an LVGL number edit control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Number edit keys:
 * `x`, `y`, `w`, `h`
 * `min`, `max`
 * `get`, `set`
 * `display` (function)
 * `edited` (function)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.choice(params)

Create an LVGL choice control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Choice keys:
 * `x`, `y`, `w`, `h`
 * `title` (string or function)
 * `values` (table of strings)
 * `get`, `set`
 * `filter` (function receiving a 1-based item index)
 * `popupWidth` (number)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.slider(params)

Create an LVGL slider control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Slider keys:
 * `x`, `y`, `w`
 * `color`
 * `min`, `max`
 * `get`, `set`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.verticalSlider(params)

Create an LVGL vertical slider control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Vertical slider keys:
 * `x`, `y`, `h`
 * `color`
 * `min`, `max`
 * `get`, `set`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.font(params)

Create an LVGL font picker control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Font picker keys:
 * `x`, `y`, `w`, `h`
 * `get`, `set`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.align(params)

Create an LVGL alignment picker control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Alignment picker keys:
 * `x`, `y`, `w`, `h`
 * `get`, `set`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.color(params)

Create an LVGL color picker control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Color picker keys:
 * `x`, `y`, `w`, `h`
 * `get`, `set`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.timer(params)

Create an LVGL timer picker control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Timer picker keys:
 * `x`, `y`, `w`, `h`
 * `get`, `set`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.switch(params)

Create an LVGL switch picker control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Switch picker keys:
 * `x`, `y`, `w`, `h`
 * `get`, `set`
 * `filter` (number mask such as `lvgl.SW_*`)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.source(params)

Create an LVGL source picker control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Source picker keys:
 * `x`, `y`, `w`, `h`
 * `get`, `set`
 * `filter` (number mask such as `lvgl.SRC_*`)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.file(params)

Create an LVGL file picker control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice File picker keys:
 * `x`, `y`, `w`, `h`
 * `title` (string or function)
 * `folder` (string)
 * `extension` (string)
 * `maxLen` (number)
 * `hideExtension` (boolean)
 * `get`, `set`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.box(params)

Create an LVGL box container.

@param params (table) object definition table

@retval table created LVGL object

@notice Box keys:
 * `x`, `y`, `w`, `h`
 * `color`, `opacity`, `visible`
 * `align`
 * `flexFlow`, `flexPad`, `borderPad`
 * `scrollBar`, `scrollDir`, `scrollTo`, `scrolled`
 * `children`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.setting(params)

Create an LVGL setting control.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Setting keys:
 * `x`, `y`, `w`, `h`
 * `title` (string or function)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.page(params)

Create an LVGL page container.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Page keys:
 * `title`, `subtitle`, `icon`
 * `back`, `menu`
 * `backButton` (boolean)
 * `prevButton`, `nextButton` tables with `press` and `active`
 * `align`
 * `scrollBar`, `scrollDir`, `scrollTo`, `scrolled`
 * `children`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.dialog(params)

Create an LVGL dialog container.

@param params (table) object definition table

@retval table created LVGL object

@notice Available only in standalone scripts and fullscreen widgets.

@notice Dialog keys:
 * `title` (string or function)
 * `w`, `h`
 * `close` (function)
 * `children`

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.confirm(params)

Show a confirmation dialog using the LVGL UI layer.

@param params (table) dialog definition table

@notice Confirm dialog keys:
 * `title` (string or function)
 * `message` (string)
 * `confirm` (function)
 * `cancel` (function)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.message(params)

Show a message dialog using the LVGL UI layer.

@param params (table) dialog definition table

@notice Message dialog keys:
 * `title` (string or function)
 * `message` (string)
 * `details` (string)

@status current Introduced in 3.0.0
*/
/*luadoc
@function lvgl.menu(params)

Show an LVGL menu popup.

@param params (table) dialog definition table

@notice Menu keys:
 * `title` (string or function)
 * `values` (table of strings)
 * `get`, `set`

@status current Introduced in 3.0.0
*/
extern "C" {
// lvgl functions
LROT_BEGIN(lvgllib, NULL, 0)
  LROT_FUNCENTRY(clear, luaLvglClear)
  LROT_FUNCENTRY(build, luaLvglBuild)
  LROT_FUNCENTRY(isAppMode, luaLvglIsAppMode)
  LROT_FUNCENTRY(isFullScreen, luaLvglIsFullscreen)
  LROT_FUNCENTRY(exitFullScreen, luaLvglExitFullscreen)
  LROT_FUNCENTRY(getContext, luaLvglGetContext)
  // Objects - widgets and standalone scripts
  LROT_FUNCENTRY(label, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetLabel(); }); })
  LROT_FUNCENTRY(rectangle, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetRectangle(); }); })
  LROT_FUNCENTRY(hline, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetHLine(); }); })
  LROT_FUNCENTRY(vline, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetVLine(); }); })
  LROT_FUNCENTRY(line, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetLine(); }); })
  LROT_FUNCENTRY(triangle, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetTriangle(); }); })
  LROT_FUNCENTRY(circle, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetCircle(); }); })
  LROT_FUNCENTRY(arc, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetArc(); }); })
  LROT_FUNCENTRY(image, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetImage(); }); })
  LROT_FUNCENTRY(qrcode, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetQRCode(); }); })
  // Objects - standalone scripts and full screen widgets only
  LROT_FUNCENTRY(button, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetTextButton(); }, true); })
  LROT_FUNCENTRY(momentaryButton, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetMomentaryButton(); }, true); })
  LROT_FUNCENTRY(toggle, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetToggleSwitch(); }, true); })
  LROT_FUNCENTRY(textEdit, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetTextEdit(); }, true); })
  LROT_FUNCENTRY(numberEdit, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetNumberEdit(); }, true); })
  LROT_FUNCENTRY(choice, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetChoice(); }, true); })
  LROT_FUNCENTRY(slider, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetSlider(); }, true); })
  LROT_FUNCENTRY(verticalSlider, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetVerticalSlider(); }, true); })
  LROT_FUNCENTRY(font, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetFontPicker(); }, true); })
  LROT_FUNCENTRY(align, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetAlignPicker(); }, true); })
  LROT_FUNCENTRY(color, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetColorPicker(); }, true); })
  LROT_FUNCENTRY(timer, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetTimerPicker(); }, true); })
  LROT_FUNCENTRY(switch, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetSwitchPicker(); }, true); })
  LROT_FUNCENTRY(source, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetSourcePicker(); }, true); })
  LROT_FUNCENTRY(file, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetFilePicker(); }, true); })
  // Containers
  LROT_FUNCENTRY(box, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetBox(); }); })
  LROT_FUNCENTRY(setting, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetSetting(); }, true); })
  LROT_FUNCENTRY(page, [](lua_State* L) { return luaLvglObj(L, []() { return new LvglWidgetPage(); }, true); })
  LROT_FUNCENTRY(dialog, [](lua_State* L) { return luaLvglObj(L, []() { return new LvglWidgetDialog(); }, true); })
  // Dialogs
  LROT_FUNCENTRY(confirm, [](lua_State* L) { return luaLvglPopup(L, []() { return new LvglWidgetConfirmDialog(); }); })
  LROT_FUNCENTRY(message, [](lua_State* L) { return luaLvglPopup(L, []() { return new LvglWidgetMessageDialog(); }); })
  LROT_FUNCENTRY(menu, [](lua_State* L) { return luaLvglPopup(L, []() { return new LvglWidgetMenu(); }); })
  // Object manipulation functions
  LROT_FUNCENTRY(set, luaLvglSet)
  LROT_FUNCENTRY(show, luaLvglShow)
  LROT_FUNCENTRY(hide, luaLvglHide)
  LROT_FUNCENTRY(enable, luaLvglEnable)
  LROT_FUNCENTRY(disable, luaLvglDisable)
  LROT_FUNCENTRY(close, luaLvglClose)
  LROT_FUNCENTRY(getScrollPos, luaLvglGetScrollPos)
  LROT_NUMENTRY(FLOW_ROW, LV_FLEX_FLOW_ROW)
  LROT_NUMENTRY(FLOW_COLUMN, LV_FLEX_FLOW_COLUMN)
  LROT_NUMENTRY(PAD_TINY, PAD_TINY)
  LROT_NUMENTRY(PAD_SMALL, PAD_SMALL)
  LROT_NUMENTRY(PAD_MEDIUM, PAD_MEDIUM)
  LROT_NUMENTRY(PAD_LARGE, PAD_LARGE)
  LROT_NUMENTRY(PAD_OUTLINE, PAD_OUTLINE)
  LROT_NUMENTRY(PAD_BORDER, PAD_BORDER)
  LROT_NUMENTRY(SRC_ALL, 0xFFFFFFFF)
  LROT_NUMENTRY(SRC_INPUT, SRC_INPUT)
  LROT_NUMENTRY(SRC_LUA, SRC_LUA)
  LROT_NUMENTRY(SRC_STICK, SRC_STICK|SRC_TILT|SRC_LIGHT|SRC_SPACEMOUSE)
  LROT_NUMENTRY(SRC_POT, SRC_POT)
  LROT_NUMENTRY(SRC_OTHER, SRC_MINMAX|SRC_TX|SRC_TIMER)
  LROT_NUMENTRY(SRC_HELI, SRC_HELI)
  LROT_NUMENTRY(SRC_TRIM, SRC_TRIM)
  LROT_NUMENTRY(SRC_SWITCH, SRC_SWITCH|SRC_FUNC_SWITCH)
  LROT_NUMENTRY(SRC_LOGICAL_SWITCH, SRC_LOGICAL_SWITCH)
  LROT_NUMENTRY(SRC_TRAINER, SRC_TRAINER)
  LROT_NUMENTRY(SRC_CHANNEL, SRC_CHANNEL)
  LROT_NUMENTRY(SRC_GVAR, SRC_GVAR)
  LROT_NUMENTRY(SRC_TELEM, SRC_TELEM)
  LROT_NUMENTRY(SRC_CLEAR, SRC_NONE)
  LROT_NUMENTRY(SRC_INVERT, SRC_INVERT)
  LROT_NUMENTRY(SW_ALL, 0xFFFFFFFF)
  LROT_NUMENTRY(SW_SWITCH, SW_SWITCH)
  LROT_NUMENTRY(SW_TRIM, SW_TRIM)
  LROT_NUMENTRY(SW_LOGICAL_SWITCH, SW_LOGICAL_SWITCH)
  LROT_NUMENTRY(SW_TELEM, SW_TELEM)
  LROT_NUMENTRY(SW_OTHER, SW_OTHER)
  LROT_NUMENTRY(SW_CLEAR, SW_NONE)
  LROT_NUMENTRY(SCROLL_OFF, LV_DIR_NONE)
  LROT_NUMENTRY(SCROLL_HOR, LV_DIR_HOR)
  LROT_NUMENTRY(SCROLL_VER, LV_DIR_VER)
  LROT_NUMENTRY(SCROLL_ALL, LV_DIR_ALL)
  LROT_NUMENTRY(PERCENT_SIZE, LV_PCT(0))
  LROT_NUMENTRY(PAGE_BODY_HEIGHT, LCD_H - EdgeTxStyles::MENU_HEADER_HEIGHT)
  LROT_NUMENTRY(UI_ELEMENT_HEIGHT, EdgeTxStyles::UI_ELEMENT_HEIGHT)
  LROT_FLOATENTRY(LCD_SCALE, LUA_LCD_SCALE)
  LROT_NUMENTRY(LABEL, ETX_LABEL)
  LROT_NUMENTRY(RECTANGLE, ETX_RECTANGLE)
  LROT_NUMENTRY(CIRCLE, ETX_CIRCLE)
  LROT_NUMENTRY(ARC, ETX_ARC)
  LROT_NUMENTRY(HLINE, ETX_HLINE)
  LROT_NUMENTRY(VLINE, ETX_VLINE)
  LROT_NUMENTRY(LINE, ETX_LINE)
  LROT_NUMENTRY(TRIANGLE, ETX_TRIANGLE)
  LROT_NUMENTRY(IMAGE, ETX_IMAGE)
  LROT_NUMENTRY(QRCODE, ETX_QRCODE)
  LROT_NUMENTRY(BOX, ETX_BOX)
  LROT_NUMENTRY(BUTTON, ETX_BUTTON)
  LROT_NUMENTRY(MOMENTARY_BUTTON, ETX_MOMENTARY_BUTTON)
  LROT_NUMENTRY(TOGGLE, ETX_TOGGLE)
  LROT_NUMENTRY(TEXT_EDIT, ETX_TEXTEDIT)
  LROT_NUMENTRY(NUMBER_EDIT, ETX_NUMBEREDIT)
  LROT_NUMENTRY(CHOICE, ETX_CHOICE)
  LROT_NUMENTRY(SLIDER, ETX_SLIDER)
  LROT_NUMENTRY(VERTICAL_SLIDER, ETX_VERTICAL_SLIDER)
  LROT_NUMENTRY(PAGE, ETX_PAGE)
  LROT_NUMENTRY(FONT, ETX_FONT)
  LROT_NUMENTRY(ALIGN, ETX_ALIGN)
  LROT_NUMENTRY(COLOR, ETX_COLOR)
  LROT_NUMENTRY(TIMER, ETX_TIMER)
  LROT_NUMENTRY(SWITCH, ETX_SWITCH)
  LROT_NUMENTRY(SOURCE, ETX_SOURCE)
  LROT_NUMENTRY(FILE, ETX_FILE)
  LROT_NUMENTRY(SETTING, ETX_SETTING)
LROT_END(lvgllib, NULL, 0)

// Metatable for simple objects (line, arc, label)
LROT_BEGIN(lvgl_base_mt, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY(__gc, luaDestroyLvglWidget)
  LROT_TABENTRY(__index, lvgl_base_mt)
  // Object manipulation functions
  LROT_FUNCENTRY(set, luaLvglSet)
  LROT_FUNCENTRY(show, luaLvglShow)
  LROT_FUNCENTRY(hide, luaLvglHide)
LROT_END(lvgl_base_mt, NULL, LROT_MASK_GC_INDEX)

// Metatable for complex objects
LROT_BEGIN(lvgl_mt, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY(__gc, luaDestroyLvglWidget)
  LROT_TABENTRY(__index, lvgl_mt)
  LROT_FUNCENTRY(clear, luaLvglClear)
  LROT_FUNCENTRY(build, luaLvglBuild)
  // Objects - widgets and standalone scripts
  LROT_FUNCENTRY(label, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetLabel(); }); })
  LROT_FUNCENTRY(rectangle, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetRectangle(); }); })
  LROT_FUNCENTRY(hline, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetHLine(); }); })
  LROT_FUNCENTRY(vline, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetVLine(); }); })
  LROT_FUNCENTRY(line, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetLine(); }); })
  LROT_FUNCENTRY(triangle, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetTriangle(); }); })
  LROT_FUNCENTRY(circle, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetCircle(); }); })
  LROT_FUNCENTRY(arc, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetArc(); }); })
  LROT_FUNCENTRY(image, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetImage(); }); })
  LROT_FUNCENTRY(qrcode, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetQRCode(); }); })
  // Objects - standalone scripts and full screen widgets only
  LROT_FUNCENTRY(button, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetTextButton(); }, true); })
  LROT_FUNCENTRY(momentaryButton, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetMomentaryButton(); }, true); })
  LROT_FUNCENTRY(toggle, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetToggleSwitch(); }, true); })
  LROT_FUNCENTRY(textEdit, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetTextEdit(); }, true); })
  LROT_FUNCENTRY(numberEdit, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetNumberEdit(); }, true); })
  LROT_FUNCENTRY(choice, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetChoice(); }, true); })
  LROT_FUNCENTRY(slider, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetSlider(); }, true); })
  LROT_FUNCENTRY(verticalSlider, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetVerticalSlider(); }, true); })
  LROT_FUNCENTRY(font, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetFontPicker(); }, true); })
  LROT_FUNCENTRY(align, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetAlignPicker(); }, true); })
  LROT_FUNCENTRY(color, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetColorPicker(); }, true); })
  LROT_FUNCENTRY(timer, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetTimerPicker(); }, true); })
  LROT_FUNCENTRY(switch, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetSwitchPicker(); }, true); })
  LROT_FUNCENTRY(source, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetSourcePicker(); }, true); })
  LROT_FUNCENTRY(file, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetFilePicker(); }, true); })
  // Containers
  LROT_FUNCENTRY(box, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetBox(); }); })
  LROT_FUNCENTRY(setting, [](lua_State* L) { return luaLvglObjEx(L, []() { return new LvglWidgetSetting(); }, true); })
  // Object manipulation functions
  LROT_FUNCENTRY(set, luaLvglSet)
  LROT_FUNCENTRY(show, luaLvglShow)
  LROT_FUNCENTRY(hide, luaLvglHide)
  LROT_FUNCENTRY(enable, luaLvglEnable)
  LROT_FUNCENTRY(disable, luaLvglDisable)
  LROT_FUNCENTRY(close, luaLvglClose)
  LROT_FUNCENTRY(getScrollPos, luaLvglGetScrollPos)
LROT_END(lvgl_mt, NULL, LROT_MASK_GC_INDEX)

LUALIB_API int luaopen_lvgl(lua_State *L)
{
  luaL_rometatable(L, LVGL_SIMPLEMETATABLE, LROT_TABLEREF(lvgl_base_mt));
  luaL_rometatable(L, LVGL_METATABLE, LROT_TABLEREF(lvgl_mt));
  return 0;
}
}
