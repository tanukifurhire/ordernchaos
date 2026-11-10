local class = DefineType("UI_HowToPlayBG", {
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
        ECS.setTransform(entity, { x = self.hiddenX, y = self.hiddenY })

        Event.subscribe("OnHowToPlayOpen", function(params)
            ECS.setTransform(entity, { x = self.visX, y = self.visY })
        end)

        Event.subscribe("OnHowToPlayClose", function(params)
            ECS.setTransform(entity, { x = self.hiddenX, y = self.hiddenY })
        end)
    end,

    OnClick = function(self, entity, x, y)
        Log.info("[UI_HowToPlayBG] Closing How To Play")
        Event.emitLocal("OnHowToPlayClose", {})
    end
})

return class