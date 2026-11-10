--------------------------------------------------------------------------------
-- UI_TextInput.lua
-- A text input field driven by keyboard polling.
--
-- Activation / deactivation is controlled via events:
--   Event.emitLocal("TextInputActivate",   { target = "<entityName>" })
--   Event.emitLocal("TextInputDeactivate", { target = "<entityName>" })
--
-- Only one text input can be active at a time.  Activating one automatically
-- deactivates whichever field currently has focus.
--
-- When the user presses Enter while the field is active, the field emits:
--   Event.emitLocal("TextInputSubmit", { target = "<entityName>", text = "..." })
--
-- Serializables:
--   maxLength  – maximum characters (inclusive of whitespace)  default 20
--   fontSize   – font size in pixels                          default 24
--------------------------------------------------------------------------------

-- GLFW key codes used directly (engine only exposes a handful of KEY_* globals)
local GLFW_KEY_BACKSPACE   = 259
local GLFW_KEY_LEFT_SHIFT  = 340
local GLFW_KEY_RIGHT_SHIFT = 344
local GLFW_KEY_CAPS_LOCK   = 280
local GLFW_KEY_PERIOD      = 46
local GLFW_KEY_COMMA       = 44
local GLFW_KEY_MINUS       = 45
local GLFW_KEY_EQUAL       = 61   -- =/+
local GLFW_KEY_SLASH       = 47
local GLFW_KEY_SEMICOLON   = 59
local GLFW_KEY_APOSTROPHE  = 39
local GLFW_KEY_LEFT_BRACKET  = 91
local GLFW_KEY_RIGHT_BRACKET = 93
local GLFW_KEY_BACKSLASH   = 92
local GLFW_KEY_GRAVE       = 96

--------------------------------------------------------------------------------
-- Helpers
--------------------------------------------------------------------------------

-- Build a table mapping GLFW key code -> { normal, shifted }
local keyMap = {}

-- Letters A-Z  (GLFW codes 65-90, same as ASCII uppercase)
for i = 65, 90 do
    local lower = string.char(i + 32) -- a-z
    local upper = string.char(i)      -- A-Z
    keyMap[i] = { lower, upper }
end

-- Digits 0-9  (GLFW codes 48-57)
local shiftedDigits = { ")", "!", "@", "#", "$", "%", "^", "&", "*", "(" }
for i = 0, 9 do
    keyMap[48 + i] = { tostring(i), shiftedDigits[i + 1] }
end

-- Space (GLFW_KEY_SPACE = 32, also registered as KEY_SPACE global)
keyMap[32]                     = { " ",  " "  }

-- Punctuation / symbol keys
keyMap[GLFW_KEY_PERIOD]        = { ".",  ">"  }
keyMap[GLFW_KEY_COMMA]         = { ",",  "<"  }
keyMap[GLFW_KEY_MINUS]         = { "-",  "_"  }
keyMap[GLFW_KEY_EQUAL]         = { "=",  "+"  }
keyMap[GLFW_KEY_SLASH]         = { "/",  "?"  }
keyMap[GLFW_KEY_SEMICOLON]     = { ";",  ":"  }
keyMap[GLFW_KEY_APOSTROPHE]    = { "'",  "\"" }
keyMap[GLFW_KEY_LEFT_BRACKET]  = { "[",  "{"  }
keyMap[GLFW_KEY_RIGHT_BRACKET] = { "]",  "}"  }
keyMap[GLFW_KEY_BACKSLASH]     = { "\\", "|"  }
keyMap[GLFW_KEY_GRAVE]         = { "`",  "~"  }

-- Collect every key code we care about so we can iterate once per frame
local allKeyCodes = { GLFW_KEY_BACKSPACE }
for code, _ in pairs(keyMap) do
    allKeyCodes[#allKeyCodes + 1] = code
end

--------------------------------------------------------------------------------
-- Key-repeat parameters (seconds) – tuned to feel like a native OS text field
--------------------------------------------------------------------------------
local REPEAT_INITIAL_DELAY = 0.45   -- hold this long before repeat kicks in
local REPEAT_INTERVAL      = 0.035  -- time between repeated characters once repeating

--------------------------------------------------------------------------------
-- Cursor blink
--------------------------------------------------------------------------------
local CURSOR_BLINK_RATE = 0.53 -- seconds per half-cycle
local CURSOR_CHAR       = "|"

--------------------------------------------------------------------------------
-- Singleton focus — only one text input may be active at a time.
-- Module-level so it is shared across all instances of this script.
--------------------------------------------------------------------------------
local activeInstance = nil   -- reference to the currently focused { self, entity }

--------------------------------------------------------------------------------
-- Component
--------------------------------------------------------------------------------
local class = DefineType("UI_TextInput", {
    Serializable = {
        maxLength = 20,
        fontSize  = 24,
        fieldName = "MyTextInput",
        enterEvent = "MyTextInputEvent",
        hint = "MyTextInputHint"
    },
    
    -- runtime state (not serialised)
    active        = false,
    text          = "",
    cursorVisible = true,
    cursorTimer   = 0,

    -- per-key repeat tracking:  keyCode -> { held = bool, timer = number, repeating = bool }
    keyStates     = nil,
    shiftHeld     = false,

    ---------------------------------------------------------------------------
    Start = function(self, entity)
        self.text       = ""
        self.active     = false
        self.keyStates  = {}
        self.cursorTimer   = 0
        self.cursorVisible = true

        -- Initialise repeat state for every key we poll
        for _, code in ipairs(allKeyCodes) do
            self.keyStates[code] = { held = false, timer = 0, repeating = false }
        end

        -- Listen for activate / deactivate
        Event.subscribe("TextInputActivate", function(params)
            if params and params.target == self.fieldName then
                self:Activate(entity)
            else
                self:Deactivate(entity)
            end
        end)

        Event.subscribe("TextInputDeactivate", function(params)
            self:Deactivate(entity)
        end)

        -- Apply initial visual
        self:RefreshDisplay(entity)
    end,

    ---------------------------------------------------------------------------
    Activate = function(self, entity)
        -- Deactivate whoever currently owns focus (if it isn't us)
        if activeInstance and activeInstance.self ~= self then
            activeInstance.self:Deactivate(activeInstance.entity)
        end

        if self.active then return end
        self.active        = true
        self.cursorTimer   = 0
        self.cursorVisible = true

        -- Claim focus
        activeInstance = { self = self, entity = entity }

        -- Reset all key states so stale held keys don't fire
        for _, code in ipairs(allKeyCodes) do
            self.keyStates[code].held      = false
            self.keyStates[code].timer     = 0
            self.keyStates[code].repeating = false
        end

        self:RefreshDisplay(entity)
    end,

    ---------------------------------------------------------------------------
    Deactivate = function(self, entity)
        if not self.active then return end
        self.active = false

        -- Release focus if we own it
        if activeInstance and activeInstance.self == self then
            activeInstance = nil
        end

        self:RefreshDisplay(entity)
    end,

    ---------------------------------------------------------------------------
    Update = function(self, entity)
        if not self.active then return end

        local dt = Time.getDeltaTime()

        -- Escape deactivates without submitting
        if Input.isKeyTriggered(KEY_ESCAPE) then
            self:Deactivate(entity)
            return
        end

        -- Enter submits and deactivates before any input is processed
        if Input.isKeyTriggered(KEY_ENTER) then
            Event.emitLocal("TextInputSubmit", {
                target = self.fieldName,
                text   = self.text
            })
            self:Deactivate(entity)
            return
        end

        -- Detect shift
        self.shiftHeld = Input.isKeyPressed(GLFW_KEY_LEFT_SHIFT)
                      or Input.isKeyPressed(GLFW_KEY_RIGHT_SHIFT)

        local dirty = false

        -- Process every mapped key
        for _, code in ipairs(allKeyCodes) do
            local ks      = self.keyStates[code]
            local pressed = Input.isKeyPressed(code)

            if pressed then
                if not ks.held then
                    -- Key just went down – always fire immediately
                    ks.held      = true
                    ks.timer     = 0
                    ks.repeating = false
                    dirty = self:HandleKey(code) or dirty
                else
                    -- Key is being held – accumulate time for repeat
                    ks.timer = ks.timer + dt
                    if not ks.repeating then
                        if ks.timer >= REPEAT_INITIAL_DELAY then
                            ks.repeating = true
                            ks.timer     = 0
                            dirty = self:HandleKey(code) or dirty
                        end
                    else
                        while ks.timer >= REPEAT_INTERVAL do
                            ks.timer = ks.timer - REPEAT_INTERVAL
                            dirty = self:HandleKey(code) or dirty
                        end
                    end
                end
            else
                -- Key released
                if ks.held then
                    ks.held      = false
                    ks.timer     = 0
                    ks.repeating = false
                end
            end
        end

        -- Emit on every text change
        if dirty then
            Event.emitLocal("TextInputSubmit", {
                target = self.fieldName,
                text   = self.text
            })
        end

        -- Cursor blink
        self.cursorTimer = self.cursorTimer + dt
        if self.cursorTimer >= CURSOR_BLINK_RATE then
            self.cursorTimer = self.cursorTimer - CURSOR_BLINK_RATE
            self.cursorVisible = not self.cursorVisible
            dirty = true
        end

        if dirty then
            self:RefreshDisplay(entity)
        end
    end,

    ---------------------------------------------------------------------------
    -- Process a single key action. Returns true if text changed.
    ---------------------------------------------------------------------------
    HandleKey = function(self, code)
        -- Backspace
        if code == GLFW_KEY_BACKSPACE then
            if #self.text > 0 then
                self.text = self.text:sub(1, #self.text - 1)
                -- Reset cursor to visible on every edit so user always sees it
                self.cursorVisible = true
                self.cursorTimer   = 0
                return true
            end
            return false
        end

        -- Character keys
        local mapping = keyMap[code]
        if mapping then
            if #self.text < self.maxLength then
                local ch = self.shiftHeld and mapping[2] or mapping[1]
                self.text = self.text .. ch
                -- Reset cursor to visible on every edit
                self.cursorVisible = true
                self.cursorTimer   = 0
                return true
            end
        end

        return false
    end,

    ---------------------------------------------------------------------------
    -- Push current text (+ cursor if active) to the TextComponent
    ---------------------------------------------------------------------------
    RefreshDisplay = function(self, entity)
        local isHint = (not self.active) and (#self.text == 0) and (self.hint ~= nil) and (#self.hint > 0)
        local display

        if isHint then
            display = self.hint
        else
            display = self.text
        end

        if self.active and self.cursorVisible and not isHint then
            display = display .. CURSOR_CHAR
        end

        local textColor = ECS.getTextComponent(entity).color
        if isHint then
            textColor.a = 0.9
        else
            textColor.a = 1.0
        end

        ECS.setTextComponent(entity, {
            text     = display,
            fontSize = self.fontSize,
            color    = textColor
        })
    end,

    ---------------------------------------------------------------------------
    -- Utility: get current text value from outside
    ---------------------------------------------------------------------------
    GetText = function(self)
        return self.text
    end,

    ---------------------------------------------------------------------------
    -- Utility: programmatically set text
    ---------------------------------------------------------------------------
    SetText = function(self, entity, newText)
        if #newText > self.maxLength then
            newText = newText:sub(1, self.maxLength)
        end
        self.text = newText
        self:RefreshDisplay(entity)
    end
})

return class
