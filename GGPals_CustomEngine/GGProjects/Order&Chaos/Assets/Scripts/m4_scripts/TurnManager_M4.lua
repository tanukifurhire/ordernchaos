-- Enhanced TurnManager with M4 Table Support
-- Now can store complex turn queue data directly in the script

local class = DefineType("TurnManager_M4", {
    Serializable = {
        currentTurn = 1,
        currentPhase = "Deployment",  -- "Deployment", "Planning", "Resolution"
        activePlayer = 1,
        maxTurns = 20
    },
    
    -- NEW M4 FEATURE: Direct table storage in non-Serializable fields
    -- No more global registry workarounds needed!
    
    turnQueue = {},  -- Array of {entity, side, initiative, isKing}
    
    turnHistory = {},  -- Log of all turns played
    
    phaseData = {
        deployment = {completed = false, unitsPlaced = 0},
        planning = {completed = false, actionsQueued = 0},
        resolution = {completed = false, actionsExecuted = 0}
    },
    
    playerState = {
        p1 = {
            ap = 3,
            unitsDeployed = 0,
            actionsPlanned = 0,
            kingSelected = false
        },
        p2 = {
            ap = 3,
            unitsDeployed = 0,
            actionsPlanned = 0,
            kingSelected = false
        }
    },
    
    pendingActions = {
        p1 = {},
        p2 = {}
    },
    
    resolutionOrder = {},  -- Computed order for resolution phase
    
    Init = function(self, entity)
        Log.info("[TurnMgr_M4] Enhanced TurnManager initialized")
    end,
    
    Start = function(self, entity)
        Log.info("[TurnMgr_M4] Starting with table support enabled")
        
        -- Subscribe to game events
        Event.subscribe("UnitDeployed", function(params)
            TurnManager_M4.OnUnitDeployed(self, entity, params)
        end)
        
        Event.subscribe("ActionPlanned", function(params)
            TurnManager_M4.OnActionPlanned(self, entity, params)
        end)
        
        Event.subscribe("EndTurn", function()
            TurnManager_M4.AdvanceTurn(self, entity)
        end)
        
        -- Initialize turn queue
        TurnManager_M4.InitializeTurnQueue(self, entity)
    end,
    
    Update = function(self, entity)
        -- Monitor game state
    end,
    
    -- ========================================
    -- TURN QUEUE MANAGEMENT
    -- ========================================
    
    InitializeTurnQueue = function(self, entity)
        -- Clear existing queue
        self.turnQueue = {}
        
        Log.info("[TurnMgr_M4] Turn queue initialized")
    end,
    
    AddToTurnQueue = function(self, entity, unitEntity, side, initiative, isKing)
        local queueEntry = {
            entity = unitEntity,
            side = side,
            initiative = initiative or 0,
            isKing = isKing or false,
            hasActed = false
        }
        
        table.insert(self.turnQueue, queueEntry)
        
        -- Sort by initiative (higher goes first)
        TurnManager_M4.SortTurnQueue(self, entity)
        
        Log.info("[TurnMgr_M4] Added unit to queue. Queue size: " .. #self.turnQueue)
    end,
    
    SortTurnQueue = function(self, entity)
        table.sort(self.turnQueue, function(a, b)
            -- Kings always go first
            if a.isKing ~= b.isKing then
                return a.isKing
            end
            -- Then by initiative
            return a.initiative > b.initiative
        end)
    end,
    
    GetNextInQueue = function(self, entity)
        for i = 1, #self.turnQueue do
            if not self.turnQueue[i].hasActed then
                return self.turnQueue[i], i
            end
        end
        return nil, -1
    end,
    
    RemoveFromQueue = function(self, entity, unitEntity)
        for i = #self.turnQueue, 1, -1 do
            if self.turnQueue[i].entity == unitEntity then
                table.remove(self.turnQueue, i)
                Log.info("[TurnMgr_M4] Removed unit from queue")
                return true
            end
        end
        return false
    end,
    
    -- ========================================
    -- PHASE MANAGEMENT
    -- ========================================
    
    OnUnitDeployed = function(self, entity, params)
        local player = params.player or 1
        local playerKey = "p" .. player
        
        self.playerState[playerKey].unitsDeployed = 
            self.playerState[playerKey].unitsDeployed + 1
        
        self.phaseData.deployment.unitsPlaced = 
            self.phaseData.deployment.unitsPlaced + 1
        
        -- Add to turn queue
        if params.unitEntity then
            TurnManager_M4.AddToTurnQueue(self, entity, params.unitEntity, 
                player, params.initiative or 0, params.isKing or false)
        end
    end,
    
    OnActionPlanned = function(self, entity, params)
        local player = params.player or 1
        local playerKey = "p" .. player
        
        -- Store the action
        table.insert(self.pendingActions[playerKey], {
            unitEntity = params.unitEntity,
            actionType = params.actionType,
            targetPos = params.targetPos,
            abilityId = params.abilityId
        })
        
        self.playerState[playerKey].actionsPlanned = 
            self.playerState[playerKey].actionsPlanned + 1
        
        self.phaseData.planning.actionsQueued = 
            self.phaseData.planning.actionsQueued + 1
    end,
    
    -- ========================================
    -- RESOLUTION PHASE
    -- ========================================
    
    ComputeResolutionOrder = function(self, entity)
        self.resolutionOrder = {}
        
        -- For each player's actions
        for playerNum = 1, 2 do
            local playerKey = "p" .. playerNum
            local actions = self.pendingActions[playerKey]
            
            for i = 1, #actions do
                local action = actions[i]
                
                -- Add to resolution order with priority
                table.insert(self.resolutionOrder, {
                    player = playerNum,
                    action = action,
                    priority = TurnManager_M4.GetActionPriority(self, entity, action)
                })
            end
        end
        
        -- Sort by priority
        table.sort(self.resolutionOrder, function(a, b)
            return a.priority > b.priority
        end)
        
        Log.info("[TurnMgr_M4] Resolution order computed: " .. #self.resolutionOrder .. " actions")
    end,
    
    GetActionPriority = function(self, entity, action)
        -- Simple priority system
        if action.actionType == "ability" then
            return 100
        elseif action.actionType == "movement" then
            return 50
        else
            return 0
        end
    end,
    
    ExecuteResolution = function(self, entity)
        TurnManager_M4.ComputeResolutionOrder(self, entity)
        
        -- Execute in order
        for i = 1, #self.resolutionOrder do
            local item = self.resolutionOrder[i]
            
            Log.info("[TurnMgr_M4] Executing P" .. item.player .. " action: " .. 
                item.action.actionType)
            
            -- Emit event for action execution
            Event.emitLocal("ExecuteAction", {
                player = item.player,
                action = item.action
            })
        end
        
        -- Clear pending actions
        self.pendingActions.p1 = {}
        self.pendingActions.p2 = {}
        
        -- Mark phase complete
        self.phaseData.resolution.completed = true
        self.phaseData.resolution.actionsExecuted = #self.resolutionOrder
    end,
    
    -- ========================================
    -- TURN ADVANCEMENT
    -- ========================================
    
    AdvanceTurn = function(self, entity)
        -- Log turn to history
        local turnRecord = {
            turn = self.currentTurn,
            phase = self.currentPhase,
            p1Actions = #self.pendingActions.p1,
            p2Actions = #self.pendingActions.p2
        }
        table.insert(self.turnHistory, turnRecord)
        
        -- Reset for next turn
        self.currentTurn = self.currentTurn + 1
        
        -- Reset player states
        self.playerState.p1.actionsPlanned = 0
        self.playerState.p2.actionsPlanned = 0
        
        -- Reset phase data
        self.phaseData.planning.completed = false
        self.phaseData.planning.actionsQueued = 0
        self.phaseData.resolution.completed = false
        self.phaseData.resolution.actionsExecuted = 0
        
        -- Reset queue
        for i = 1, #self.turnQueue do
            self.turnQueue[i].hasActed = false
        end
        
        Log.info("[TurnMgr_M4] Advanced to turn " .. self.currentTurn)
        
        -- Check win conditions
        if self.currentTurn > self.maxTurns then
            Event.emitLocal("GameOver", {reason = "max_turns"})
        end
    end,
    
    -- ========================================
    -- STATE QUERIES
    -- ========================================
    
    GetQueueSize = function(self, entity)
        return #self.turnQueue
    end,
    
    GetPlayerAP = function(self, entity, player)
        local playerKey = "p" .. player
        return self.playerState[playerKey].ap
    end,
    
    GetTurnHistory = function(self, entity)
        return self.turnHistory
    end,
    
    GetPhaseStatus = function(self, entity)
        return {
            deployment = self.phaseData.deployment.completed,
            planning = self.phaseData.planning.completed,
            resolution = self.phaseData.resolution.completed
        }
    end,
    
    PrintDebugInfo = function(self, entity)
        Log.info("[TurnMgr_M4] ===== DEBUG INFO =====")
        Log.info("[TurnMgr_M4] Turn: " .. self.currentTurn)
        Log.info("[TurnMgr_M4] Phase: " .. self.currentPhase)
        Log.info("[TurnMgr_M4] Queue Size: " .. #self.turnQueue)
        Log.info("[TurnMgr_M4] P1 AP: " .. self.playerState.p1.ap)
        Log.info("[TurnMgr_M4] P2 AP: " .. self.playerState.p2.ap)
        Log.info("[TurnMgr_M4] P1 Actions: " .. #self.pendingActions.p1)
        Log.info("[TurnMgr_M4] P2 Actions: " .. #self.pendingActions.p2)
        Log.info("[TurnMgr_M4] History Entries: " .. #self.turnHistory)
        Log.info("[TurnMgr_M4] ======================")
    end
})

return class
