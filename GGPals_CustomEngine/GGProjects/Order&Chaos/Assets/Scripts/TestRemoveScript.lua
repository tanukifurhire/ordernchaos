local class = DefineType("TestRemoveScript", {
    Serializable = {
        testField = 0.001,
        testField2 = false
    },

    AudioAsset = {
        testAudio = 0,
        testAudio2 = 0,
        testAudio3 = 0,
        testAudio4 = 0,
        testAudio5 = 0,
        testAudio6 = 0,
        testAudio7 = 0,
    },

    TextureAsset = {
        testTex = 0,
        testTex2 = 0,
        testTex3 = 0,
        testTex4 = 0,
        testTex5 = 0,
        testTex6 = 0,
        testTex7 = 0
    },

    Start = function(self, entity)
        for _, e in ipairs(ECS.getChildren(entity)) do
            ECS.destroyEntityDeferred(e)
        end
    end
})

return class