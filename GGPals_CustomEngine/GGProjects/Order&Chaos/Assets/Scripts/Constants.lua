-- Define constants as globals first
CONSTANTS = {
    -- Math stuff
    EPSILON = 0.00001,

    -- Speed
    MOVE_SPEED_NORMAL = 5.0,

    -- Grid dimensions
    GRID_ROWS = 4,
    GRID_COLS = 6,

    TILE_RED = 0,
    TILE_BLUE = 1,

    TILEMAP_PERSPECTIVE_FACTOR = 0.2,

    -- Color section
    COLOR_NONE =
    {
        r = 0.0,
        g = 0.0,
        b = 0.0,
        a = 0.0
    },

    COLOR_WHITE =
    {
        r = 1.0,
        g = 1.0,
        b = 1.0,
        a = 1.0
    },

    COLOR_BUTTON_HOVER_MULTIPLY = {
        r = 1.0,
        g = 1.0,
        b = 1.0,
        a = 0.15
    },

    COLOR_ABILITY_HOVER_MULTIPLY = {
        r = 1.0,
        g = 1.0,
        b = 1.0,
        a = 0.75
    },

    COLOR_HOVER_MULTIPLY = {
        r = 0.8,
        g = 0.8,
        b = 0.8,
        a = 1.0
    },

    COLOR_NORMAL_MULTIPLY = {
        r = 1.0,
        g = 1.0,
        b = 1.0,
        a = 1.0
    },

    COLOR_PLACEMENT_HIGHLIGHT = {
        r = 0.3,
        g = 0.3,
        b = 0.3,
        a = 1.0
    },
    
    COLOR_MOVEMENT_HIGHLIGHT = {
        r = 0.3,
        g = 0.3,
        b = 0.3,
        a = 1.0
    },

    COLOR_OFFENSIVE_ABILITY_HIGHLIGHT = {
        r = 0.6,
        g = 0.2,
        b = 0.1,
        a = 1.0
    },

    COLOR_SUPPORT_ABILITY_HIGHLIGHT = {
        r = 0.0,
        g = 0.0,
        b = 0.7,
        a = 1.0
    },

    COLOR_TRAP_INDICATOR = {
        r = 0.5,
        g = 0.2,
        b = 0.6,
        a = 1.0
    },

    COLOR_TILE_BLUE = 
    {
        r = 0.0,
        g = 0.0,
        b = 1.0,
        a = 1.0
    },

    COLOR_TILE_RED = 
    {
        r = 1.0,
        g = 0.0,
        b = 0.0,
        a = 1.0
    },

    COLOR_TEXT_FIELD_NORMAL_MULTIPLY = 
    {
        r = 0.392,
        g = 0.392,
        b = 0.392,
        a = 1.0
    },

    COLOR_TEXT_FIELD_HOVER_MULTIPLY = 
    {
        r = 0.2,
        g = 0.2,
        b = 0.2,
        a = 1.0
    },

    ORDER_GRID_POS = 
    {
        x = -3.0,
        y = 1.4
    },

    CHAOS_GRID_POS = 
    {
        x = 7.3,
        y = 1.4
    },

    -- Gameplay section
    PHASE =
    {
        PHASE_NONE = 0,
        DEPLOYMENT = 1,
        PLANNING = 2,
        RESOLUTION = 3,
        MAX_PHASES = 4
    },

    ORDER_PLAYER = 1,
    CHAOS_PLAYER = 2,
    ALL_PLAYERS = 3,

    UNIT = 
    {
        NONE = 0,
        DRAGOON = 1,
        ORACLE = 2,
        TIRION = 3,
        IMP = 4,
        EYE = 5,
        VIZOM = 6
    },

    ABILITY_OFFENSIVE = 0,
    ABILITY_SUPPORT = 1,

    MAX_AP = 6,

    MAX_HP = 100,
    TILE_MAX_HEALTH = 7,

    PREP_PHASE_TIMER = 90,
    MOVEMENT_AP_COST = 1,

    -- Action types for queue entries
    ACTION_MOVEMENT = 0,
    ACTION_ABILITY = 1,

    ABILITIES = 
    {
        ORACLE_1 = 1,
        ORACLE_2 = 2,
        DRAGOON_1 = 3,
        DRAGOON_2 = 4,
        IMP_1 = 5,
        IMP_2 = 6,
        EYE_1 = 7,
        EYE_2 = 8,
        TIRION_1 = 9,
        TIRION_2 = 10,
        VIZOM_1 = 11,
        VIZOM_2 = 12,
        MOVEMENT = 13,
        GENERIC_1 = 14,
        GENERIC_2 = 15,
        PASSIVE_1 = 16,
        PASSIVE_2 = 17,
        PASSIVE_3 = 18
    },

    LOBBY = 
    {
        NONE = -1
    },

    CLIENT =
    {
        INVALID = -1
    },

    -- Maps arsenal slot IDs (1-12) to the unit type that owns that slot
    -- Must match UI_Arsenal.unitSlotOriginalIDs mapping
    ABILITY_SLOT_TO_UNIT =
    {
        [1]  = 1, [2]  = 1,   -- Dragoon (unitSlotOriginalIDs[DRAGOON=1] = {1,2})
        [3]  = 2, [4]  = 2,   -- Oracle  (unitSlotOriginalIDs[ORACLE=2] = {3,4})
        [5]  = 4, [6]  = 4,   -- Imp
        [7]  = 5, [8]  = 5,   -- Eye
        [9]  = 3, [10] = 3,   -- Tirion
        [11] = 6, [12] = 6    -- Vizom
    },

    -- Unit type to display name (for UI text like Hunter's Mark)
    UNIT_NAMES =
    {
        [1] = "Dragoon",
        [2] = "Oracle",
        [3] = "Tirion",
        [4] = "Imp",
        [5] = "Eye",
        [6] = "Vizom"
    },

    -- Ability ID -> Arsenal icon prefab name (shared by all slot scripts)
    ARSENAL_ICON_PREFABS =
    {
        [1]  = "CrystalCascadeArsenalIcon",      -- ORACLE_1
        [2]  = "FiberglassFinesseArsenalIcon",    -- ORACLE_2
        [3]  = "AzureBlazeArsenalIcon",           -- DRAGOON_1
        [4]  = "FeatherCycloneArsenalIcon",       -- DRAGOON_2
        [5]  = "ImpPactArsenalIcon",              -- IMP_1
        [6]  = "WickedShiftArsenalIcon",          -- IMP_2
        [7]  = "RuinousArsenalIcon",              -- EYE_1
        [8]  = "MarrowguardArsenalIcon",          -- EYE_2
        [9]  = "LuminousSlashArsenalIcon",        -- TIRION_1
        [10] = "ConsecrationArsenalIcon",         -- TIRION_2
        [11] = "UmbralSlashArsenalIcon",          -- VIZOM_1
        [12] = "MalevolenceArsenalIcon",          -- VIZOM_2
        -- [13] MOVEMENT: no icon needed
        [14] = "CrimsonSlashArsenalIcon",         -- GENERIC (Crimson Slash)
        [15] = "SapphireSpiralArsenalIcon",       -- GENERIC (Sapphire Spiral)
        [16] = "OverclockTriggerArsenalIcon",     -- PASSIVE (Overclock Trigger)
        [17] = "HuntersMarkArsenalIcon",           -- PASSIVE (Hunter's Mark)
        [18] = "IsolationExecutionArsenalIcon"   -- PASSIVE (Isolation Execution)
    },

    -- Status ID -> Status prefab name (keyed by the ability/passive ID that inflicts it)
    STATUS_PREFABS =
    {
        [2]  = "UnitShield",    -- Oracle_1 (Fiberglass Finesse) grants Shield
        [3]  = "UnitBurn",      -- Dragoon_1 (Azure Blaze) inflicts Burn
        [12] = "UnitBleed",     -- Vizom_2 (Sigil of Malevolence) inflicts Bleed
        [17] = "UnitMark",      -- Passive_2 (Hunter's Mark) inflicts Mark
        [9]  = "UnitFervor",    -- Tirion_1 (Luminous Cleave) grants Fervor
        [8]  = "UnitWrath",     -- Eye_2 (Fractured Earth) grants Wrath
        [11] = "UnitBerserk"    -- Vizom_1 (Umbral Cleave) grants Berserk
    }
}

setmetatable(CONSTANTS, {
    __newindex = function(t, key, value)
        error("Cannot modify CONSTANTS")
    end
})

ABILITY_COST = 1  -- mutable, updated dynamically

local class = DefineType("Constants", {
    Start = function(self, entity)
        Log.info("[Constants] Loaded successfully")
    end,
    
    Update = function(self, entity)
    end
})

return class
