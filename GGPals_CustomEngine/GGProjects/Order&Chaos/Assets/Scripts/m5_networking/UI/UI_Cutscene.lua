local class = DefineType("UI_Cutscene", {
    Serializable = {
        visibleX    = 0.0,
        visibleY    = 0.0,
        hiddenX     = -10000.0,
        hiddenY     = -10000.0,

        textX       = 0.0,
        textY       = 0.0,

        fontAssetID = 1,
        fontSize    = 24,
        textMaxWidth = 800.0,
    },

    TextureAsset = {
        artTexture1 = 0,
        artTexture2 = 0,
        artTexture3 = 0,
        artTexture4 = 0,
    },

    lines = {
        "In the world of Oudain, humanity was thriving with generations pushing breakthroughs in technology.",
        "With technology at the apex of imagination, humans harvested spiritual energy lingering in locations with strong emotions.",
        "Harnessing spiritual energy, imaginary beings of myth and legend are born by augmentation to serve humanity's cause.",
        "However, each harvest comes at a great cost.....",
        "Places once of happy memories and worship",
        "Have turned into neglect and deprivation of spiritual energy.",
        "Such places have churned into something sinister, creeping and corrupting inanimate objects into gruesome beings.",
        "With corruption spreading incessantly throughout the world, many cower in fear.",
        "In fear of the unknown threat, humanity gave them a name: Chaos, the harbingers sent to destroy mankind.",
        "Countries once full of life became hollow husks, twisted into festering hives of bloodthirsty fiends.",
        "But humanity would not fall. Not yet.",
        "The survivors gathered what remained of their strength and built floating fortresses in the sky.",
        "These sanctuaries became known as the Skycastles, rising above the land where Chaos could not easily reach.",
        "The forces of Chaos soon turned their sights to the Skycastles.",
        "They seized what they could, corrupting the floating fortresses into twisted strongholds known as Dreadcastles, where their kin now gather and multiply.",
        "From the skies above the ruined world, Order and Chaos wage an endless war.",
        "Barrages of projectiles and torrents of magic tear through the skies as both sides struggle to remain afloat.",
        "Yet amidst this endless conflict, one question remains.",
        "Where will you stand? The flow of fate now rests in your hands.",
        "Order, or Chaos?",
    },

    isVisible   = false,
    currentLine = 0,

    -- One slot per line (20 total)
    line1  = 0, line2  = 0, line3  = 0, line4  = 0, line5  = 0,
    line6  = 0, line7  = 0, line8  = 0, line9  = 0, line10 = 0,
    line11 = 0, line12 = 0, line13 = 0, line14 = 0, line15 = 0,
    line16 = 0, line17 = 0, line18 = 0, line19 = 0, line20 = 0,

    Init = function(self, entity)
        self.openCooldown = 0
        ECS.setTransform(entity, { x = self.hiddenX, y = self.hiddenY })

        Event.subscribe("OnCutsceneOpen", function(params)
            UI_Cutscene.Show(self, entity)
        end)
    end,

    Start = function(self, entity)
        for i = 1, #UI_Cutscene.lines do
            local e = ECS.createEntity()
            ECS.addTransform(e, self.textX, self.textY, 1, 1, 0)
            ECS.addTextComponent(e, UI_Cutscene.lines[i], self.fontAssetID, self.fontSize)
            local tc = ECS.getTextComponent(e)
            if tc then
                tc.visible = false
                tc.maxWidth = self.textMaxWidth
                ECS.setTextComponent(e, tc)
            end
            UI_Cutscene.SetLineEntity(self, i, e)
        end
    end,

    SetLineEntity = function(self, i, e)
        if     i == 1  then self.line1  = e
        elseif i == 2  then self.line2  = e
        elseif i == 3  then self.line3  = e
        elseif i == 4  then self.line4  = e
        elseif i == 5  then self.line5  = e
        elseif i == 6  then self.line6  = e
        elseif i == 7  then self.line7  = e
        elseif i == 8  then self.line8  = e
        elseif i == 9  then self.line9  = e
        elseif i == 10 then self.line10 = e
        elseif i == 11 then self.line11 = e
        elseif i == 12 then self.line12 = e
        elseif i == 13 then self.line13 = e
        elseif i == 14 then self.line14 = e
        elseif i == 15 then self.line15 = e
        elseif i == 16 then self.line16 = e
        elseif i == 17 then self.line17 = e
        elseif i == 18 then self.line18 = e
        elseif i == 19 then self.line19 = e
        elseif i == 20 then self.line20 = e
        end
    end,

    GetLine = function(self, i)
        if     i == 1  then return self.line1
        elseif i == 2  then return self.line2
        elseif i == 3  then return self.line3
        elseif i == 4  then return self.line4
        elseif i == 5  then return self.line5
        elseif i == 6  then return self.line6
        elseif i == 7  then return self.line7
        elseif i == 8  then return self.line8
        elseif i == 9  then return self.line9
        elseif i == 10 then return self.line10
        elseif i == 11 then return self.line11
        elseif i == 12 then return self.line12
        elseif i == 13 then return self.line13
        elseif i == 14 then return self.line14
        elseif i == 15 then return self.line15
        elseif i == 16 then return self.line16
        elseif i == 17 then return self.line17
        elseif i == 18 then return self.line18
        elseif i == 19 then return self.line19
        elseif i == 20 then return self.line20
        end
        return nil
    end,

    -- Lines 1-5  → artTexture1
    -- Lines 6-10 → artTexture2
    -- Lines 11-15→ artTexture3
    -- Lines 16-20→ artTexture4
    -- Adjust the boundary numbers here if your art panels cut differently.
    GetPanelTexture = function(self, i)
        if     i <= 5  then return self.artTexture1
        elseif i <= 10 then return self.artTexture2
        elseif i <= 15 then return self.artTexture3
        else                return self.artTexture4
        end
    end,

    SetVisible = function(self, i, visible)
        local e = UI_Cutscene.GetLine(self, i)
        if not e then return end
        local tc = ECS.getTextComponent(e)
        if tc then
            tc.visible = visible
            ECS.setTextComponent(e, tc)
        end
    end,

    Show = function(self, entity)
        self.isVisible    = true
        self.currentLine  = 0
        self.openCooldown = 10
        ECS.setTransform(entity, { x = self.visibleX, y = self.visibleY })
        for i = 1, #UI_Cutscene.lines do
            UI_Cutscene.SetVisible(self, i, false)
        end
        UI_Cutscene.ShowNextLine(self, entity)
        Log.info("[UI_Cutscene] Opened")
    end,

    Hide = function(self, entity)
        self.isVisible = false
        ECS.setTransform(entity, { x = self.hiddenX, y = self.hiddenY })
        for i = 1, #UI_Cutscene.lines do
            UI_Cutscene.SetVisible(self, i, false)
        end
        Log.info("[UI_Cutscene] Hidden")
    end,

    ShowNextLine = function(self, entity)
        if self.currentLine > 0 then
            UI_Cutscene.SetVisible(self, self.currentLine, false)
        end
        self.currentLine = self.currentLine + 1
        if self.currentLine > #UI_Cutscene.lines then
            UI_Cutscene.Hide(self, entity)
            Event.emitLocal("ShowLogo", {})
            Event.emitLocal("OnCutsceneDone", {})
            Log.info("[UI_Cutscene] Done")
            return
        end
        UI_Cutscene.SetVisible(self, self.currentLine, true)
        local tex = UI_Cutscene.GetPanelTexture(self, self.currentLine)
        if tex ~= 0 then
            ECS.setSprite(entity, { textureAssetID = tex })
        end
        Log.info("[UI_Cutscene] Line " .. self.currentLine)
    end,

    Update = function(self, entity)
        if not self.isVisible then return end

        if self.openCooldown > 0 then
            self.openCooldown = self.openCooldown - 1
            return
        end

        if Input.isKeyTriggered(KEY_CTRL) then
            UI_Cutscene.Hide(self, entity)

            Event.emitLocal("ShowLogo", {})
            Event.emitLocal("OnCutsceneDone", {})
            Log.info("[UI_Cutscene] Skipped")
            return
        end

        if Input.isMouseButtonTriggered(MOUSE_LEFT) then
            UI_Cutscene.ShowNextLine(self, entity)
        end
    end
})

return class