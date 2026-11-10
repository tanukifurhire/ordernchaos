-- UI_CutsceneSkipHint.lua
-- Attach to an entity that has a TextComponent pre-set to "Press Ctrl to Skip"
-- in the editor. The entity sits at its scene position when visible,
-- and is moved off-screen when the cutscene is not active.

local class = DefineType("UI_CutsceneSkipHint", {
    Serializable = {
        hiddenX = -10000.0,
        hiddenY = -10000.0,
    },

    visX = 0.0,
    visY = 0.0,

    Init = function(self, entity)
        local t = ECS.getTransform(entity)
        self.visX = t.x
        self.visY = t.y
        -- Start hidden
        ECS.setTransform(entity, { x = self.hiddenX, y = self.hiddenY })

        Event.subscribe("OnCutsceneOpen", function(params)
            ECS.setTransform(entity, { x = self.visX, y = self.visY })
        end)

        Event.subscribe("OnCutsceneDone", function(params)
            ECS.setTransform(entity, { x = self.hiddenX, y = self.hiddenY })
        end)
    end,
})

return class
