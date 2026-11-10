--[[************************************************************************
  \file       FloatingTextManager.lua
  \author     
  \par        Email: 
  \date       01 February 2026
  \version    1.0
  \brief
    EDIT YOUR BRIEF HERE
  \details
    EDIT YOUR DETAILS HERE
  \dependencies
    
  \see
    
  \history
    - 1.0 (01 February 2026): Created this script.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
************************************************************************--]]
local class = DefineType("FloatingTextManager",
{
    Serializable =
    {
        damageFontID  =  1,   -- set your font asset ID here
        fontSize      = 80,
        scaleX        = 0.005,
        scaleY        = 0.01,
        lifetime     = 1.5,
        holdDuration = 0.5,   -- time text floats up at full opacity before fading begins
        floatSpeed   = 1,
        errorLifetime = 3.0,
        errorFontSize = 60,
        digitWidth   = 1.0,
        digitHeight  = 1.0,
        digitSpacing = 0.06,
        digitAdvance = 0.45,  -- center-to-center distance between digits (independent of sprite scale)
    },
   TextureAsset =
    {
        digitID_0 = 0,
        digitID_1 = 0,
        digitID_2 = 0,
        digitID_3 = 0,
        digitID_4 = 0,
        digitID_5 = 0,
        digitID_6 = 0,
        digitID_7 = 0,
        digitID_8 = 0,
        digitID_9 = 0,
    },


    activeTexts = {},
    spawnHandler = 0,
    errorHandler = 0,

     GetDigitAssetID = function(self, digit)
        local map = {
            ["0"] = self.digitID_0,
            ["1"] = self.digitID_1,
            ["2"] = self.digitID_2,
            ["3"] = self.digitID_3,
            ["4"] = self.digitID_4,
            ["5"] = self.digitID_5,
            ["6"] = self.digitID_6,
            ["7"] = self.digitID_7,
            ["8"] = self.digitID_8,
            ["9"] = self.digitID_9,
        }
        local id = map[digit] or 0
        Log.info("[FLoatingTextManager] GetDigitAssetID: digit='" .. digit .. "' -> textureID=" .. tostring(id))
        return id
    end,

    -- Per-digit width multipliers for kerning (relative to digitWidth)
    -- Narrow digits like 1 get less space, wide digits like 0/M get full space
    digitKerning = {
        ["0"] = 1.0,
        ["1"] = 0.6,
        ["2"] = 0.9,
        ["3"] = 0.9,
        ["4"] = 0.95,
        ["5"] = 0.9,
        ["6"] = 0.95,
        ["7"] = 0.85,
        ["8"] = 0.95,
        ["9"] = 0.95,
    },

    GetDigitKerning = function(self, digit)
        return self.digitKerning[digit] or 1.0
    end,


    Init = function(self, entity)
        -- Clean up existing subscription
        if self.spawnHandler > 0 then Event.unsubscribe(self.spawnHandler) end
        if self.errorHandler > 0 then Event.unsubscribe(self.errorHandler) end

        -- Listen for SpawnFloatingText events from abilities
        -- Expects params: { worldX, worldY, damage }
        self.spawnHandler = Event.subscribe("SpawnFloatingText", function(params)
            if not params.worldX or not params.worldY or params.damage == nil then
                Log.error("[FloatingTextManager] Missing params! Need worldX, worldY, damage")
                return
            end
            FloatingTextManager.Spawn(self, params.worldX, params.worldY, params.damage, params.text)
        end)

        -- Listen for SpawnErrorText events for screen-space error messages
        -- Expects params: { message }
        self.errorHandler = Event.subscribe("SpawnErrorText", function(params)
            if params and params.message then
                self:SpawnError(params.message)
            end
        end)
    end,

    Start = function(self, entity)
        
    end,

    SpawnError = function(self, message)
        local e = ECS.createEntity()
        if not e then return end

        -- Screen-space coordinates: (0, 0) is center, top is +Y
        -- Place near the top-middle of the screen
        -- maxWidth = 1600 spans the full screen; CENTER alignment centers within it
        local spawnX = 0
        local spawnY = 300.0

        local textColor = { r = 1.0, g = 0.3, b = 0.3, a = 1.0 }

        -- Screen-space transform uses scale (1,1), not world-space tiny scales
        ECS.addTransform(e, spawnX, spawnY, 1.0, 1.0, 0.0)
        ECS.addTextComponent(e, message, self.damageFontID, self.errorFontSize)
        ECS.setTextComponent(e, {
            color = textColor,
            alignment = 1,           -- CENTER
            verticalAlignment = 1,   -- TOP
            maxWidth = 1600,         -- full screen width so CENTER works
            layer = 25,
            visible = true
        })

        table.insert(self.activeTexts, {
            entity       = e,
            startX       = spawnX,
            startY       = spawnY,
            elapsed      = 0.0,
            lifetime     = self.errorLifetime,
            floatSpeed   = 30.0,     -- screen-space pixels per second
            color        = textColor,
            isScreenText = true
        })
    end,

    Spawn = function(self, worldX, worldY, damage, customText)

        Log.info("[FLoatingTextManager] Spawn called: damage=" .. tostring(damage) .. " customText=" .. tostring(customText))

        -- Spawn damage sprites (offset right if customText is also present)
        if damage and damage ~= 0 then
            local spriteOffsetX = 0
            if customText then
                spriteOffsetX = #customText * self.digitSpacing * 2 + self.digitSpacing * 4
            end
            self:SpawnDamageSprites(worldX + spriteOffsetX, worldY, damage)
        end

        -- Spawn custom text if provided
        if customText then
            local spawnX = worldX
            local spawnY = worldY

            local textColor
            if customText == "BURN" or customText == "Burn" then
                textColor = { r = 0.3, g = 0.7, b = 1.0, a = 1.0 }
            elseif customText == "BLEED" or customText == "Bleed" then
                textColor = { r = 0.9, g = 0.2, b = 0.2, a = 1.0 }
            elseif customText == "BLOCKED" then
                textColor = { r = 0.7, g = 0.7, b = 0.7, a = 1.0 }
            else
                textColor = { r = 1.0, g = 1.0, b = 1.0, a = 1.0 }
            end

            local e = ECS.createEntity()
            if not e then
                Log.error("[FloatingTextManager] Failed to create entity!")
                return
            end

            ECS.addTransform(e, spawnX, spawnY, self.scaleX, self.scaleY, 0.0)
            ECS.addWorldTextComponent(e, customText, self.damageFontID, self.fontSize)
            ECS.setWorldTextComponent(e, {
                color   = textColor,
                layer = 20,
                visible = true
            })

            Log.info("[FloatingTextManager] Spawned '" .. customText .. "' at world X:" .. spawnX .. " Y:" .. spawnY)

            table.insert(self.activeTexts, {
                entity       = e,
                startX       = spawnX,
                startY       = spawnY,
                elapsed      = 0.0,
                lifetime     = self.lifetime,
                holdDuration = self.holdDuration,
                floatSpeed   = self.floatSpeed,
                color        = textColor
            })
        end
    end,

    SpawnDamageSprites = function(self, worldX, worldY, damage)
        Log.info("[FLoatingTextManager] SpawnDamageSprites called: damage=" .. tostring(damage) .. " worldX=" .. tostring(worldX) .. " worldY=" .. tostring(worldY))

        if damage == 0 then
            Log.info("FLoatingTextManager damage=0, skipping spawn")
            return
        end
        
        local numStr = tostring(damage)

        -- Check all textures are valid before spawning anything
    for i = 1, #numStr do
        local digit = numStr:sub(i, i)
        local textureID = self:GetDigitAssetID(digit)
        if textureID == 0 then
            Log.warning("[FLoatingTextManager] textureID=0 for digit '" .. digit .. "' - falling back to text spawn")
            self:Spawn(worldX, worldY, damage, numStr)
            return
        end
    end

       
        -- Calculate total width using per-digit kerning and digitAdvance
        local totalAdvance = 0
        for i = 1, #numStr do
            local digit = numStr:sub(i, i)
            totalAdvance = totalAdvance + self.digitAdvance * self:GetDigitKerning(digit)
            if i < #numStr then
                totalAdvance = totalAdvance + self.digitSpacing
            end
        end
        local cursorX = worldX - totalAdvance * 0.5

        local entities = {}

        for i = 1, #numStr do
            local digit = numStr:sub(i, i)
            local textureID = self:GetDigitAssetID(digit)
            local kernedAdvance = self.digitAdvance * self:GetDigitKerning(digit)
            local digitX = cursorX + kernedAdvance * 0.5

            local e = ECS.createEntity()
            if not e then
                Log.error("[FLoatingTextManager] Failed to create entity for digit '" .. digit .. "'!")
            else
                ECS.addTransform(e, digitX, worldY, self.digitWidth, self.digitHeight, 0.0)
                ECS.addMeshRenderer(e, 0)
                ECS.addSprite(e, textureID)
                ECS.setMeshRenderer(e, { layer = 20 })
                table.insert(entities, e)
            end

            cursorX = cursorX + kernedAdvance + self.digitSpacing
        end

        Log.info("[FLoatingTextManager] SpawnDamageSprites complete: " .. #entities .. " entities created")

        table.insert(self.activeTexts, {
            entities     = entities,
            isSprite     = true,
            startY       = worldY,
            elapsed      = 0.0,
            lifetime     = self.lifetime,
            holdDuration = self.holdDuration,
            floatSpeed   = self.floatSpeed,
        })
    end,

    Update = function(self, entity)
        -- Check for pending error messages from previous scene
        if Persistent.lastError then
            local msg = Persistent.lastError
            Persistent.lastError = nil
            Log.info("[FloatingTextManager] Displaying error: " .. msg)
            self:SpawnError(msg)
        end

        local dt = Time.getDeltaTime()
        local i = 1

        while i <= #self.activeTexts do
            local d = self.activeTexts[i]
            d.elapsed = d.elapsed + dt

            -- Hold phase: text floats up at full opacity for holdDuration
            -- Fade phase: after hold expires, alpha fades from 1 → 0 over remaining lifetime
            local hold = d.holdDuration or 0
            local alpha = 1.0
            if d.elapsed > hold then
                local fadeTime = d.lifetime - hold
                if fadeTime > 0 then
                    alpha = math.max(0.0, 1.0 - (d.elapsed - hold) / fadeTime)
                else
                    alpha = 0.0
                end
            end

        if d.isSprite then
             for _, e in ipairs(d.entities) do
                local t = ECS.getTransform(e)
                ECS.setTransform(e, {
                    x = t.x, y = d.startY + d.floatSpeed * d.elapsed
                })
                 ECS.setMeshRenderer(e, {
                    colorMultiply = { r = 1.0, g = 1.0, b = 1.0, a = alpha }
                })
                end

                if d.elapsed >= d.lifetime then
                   Log.info("[FLoatingTextManager] Sprite group expired, destroying " .. #d.entities .. " entities")
                    for _, e in ipairs(d.entities) do
                        ECS.destroyEntityDeferred(e)
                    end
                    table.remove(self.activeTexts, i)
                else
                    i = i + 1
                end

        else
            -- Float upward
            local t = ECS.getTransform(d.entity)

            ECS.setTransform(d.entity, {
                x = t.x,
                y = d.startY + d.floatSpeed * d.elapsed
            })

            local fadeColor = {
                r = d.color.r,
                g = d.color.g,
                b = d.color.b,
                a = alpha
            }
            if d.isScreenText then
                ECS.setTextComponent(d.entity, { color = fadeColor })
            else
                ECS.setWorldTextComponent(d.entity, { color = fadeColor })
            end

            if d.elapsed >= d.lifetime then
                ECS.destroyEntityDeferred(d.entity)
                table.remove(self.activeTexts, i)
            else
                i = i + 1
            end
        end

       end
    end
})

Persistent = Persistent or {}

return class
