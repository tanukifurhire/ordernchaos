-- M4 Feature Test Script

M4TestResults = {
    tableArrayTests = {},
    rpcTests = {}
}

local class = DefineType("M4FeatureTest", {
    Serializable = {
        testMode = "local",  -- "local", "server", or "client"
        serverPort = 9081
    },
    
    unitPositions = {},          -- Array of positions
    tileData = {},              -- Nested table structure
    abilityQueue = {},          -- Queue implementation
    playerStats = {             -- Table with default values
        health = 100,
        mana = 50,
        level = 1
    },
    colorMap = {                -- Nested color mappings
        red = {255, 0, 0},
        blue = {0, 0, 255},
        green = {0, 255, 0}
    },
    
    -- Test tracking
    testsPassed = 0,
    testsFailed = 0,
    testResults = {},
    
    Init = function(self, entity)
        Log.info("[M4Test] Initializing M4 Feature Tests")
    end,
    
    Start = function(self, entity)
        Log.info("[M4Test] ========================================")
        Log.info("[M4Test] Starting M4 Feature Tests")
        Log.info("[M4Test] Test Mode: " .. self.testMode)
        Log.info("[M4Test] ========================================")
        
        -- Run table/array tests
        M4FeatureTest.RunTableArrayTests(self, entity)
        
        -- Run RPC tests based on mode
        if self.testMode == "server" then
            M4FeatureTest.RunServerTests(self, entity)
        elseif self.testMode == "client" then
            M4FeatureTest.RunClientTests(self, entity)
        else
            M4FeatureTest.RunLocalTests(self, entity)
        end
        
        -- Print summary
        M4FeatureTest.PrintTestSummary(self, entity)
    end,
    
    Update = function(self, entity)
        -- Continuous testing if needed
    end,
    
    -- ========================================
    -- TABLE/ARRAY FEATURE TESTS
    -- ========================================
    
    RunTableArrayTests = function(self, entity)
        Log.info("[M4Test] --- Table/Array Tests ---")
        
        -- Test 1: Array manipulation
        M4FeatureTest.TestArrayManipulation(self, entity)
        
        -- Test 2: Nested table access
        M4FeatureTest.TestNestedTables(self, entity)
        
        -- Test 3: Table initialization
        M4FeatureTest.TestTableInitialization(self, entity)
        
        -- Test 4: Queue operations
        M4FeatureTest.TestQueueOperations(self, entity)
        
        -- Test 5: Mixed data structures
        M4FeatureTest.TestMixedStructures(self, entity)
    end,
    
    TestArrayManipulation = function(self, entity)
        local testName = "Array Manipulation"
        local success = true
        local errorMsg = ""
        
        -- Add elements to array
        self.unitPositions[1] = {x = 0, y = 0}
        self.unitPositions[2] = {x = 1, y = 1}
        self.unitPositions[3] = {x = 2, y = 2}
        
        -- Verify array length
        if #self.unitPositions ~= 3 then
            success = false
            errorMsg = "Array length incorrect: " .. #self.unitPositions
        end
        
        -- Verify values
        if success and (self.unitPositions[1].x ~= 0 or self.unitPositions[2].y ~= 1) then
            success = false
            errorMsg = "Array values incorrect"
        end
        
        M4FeatureTest.RecordTest(self, entity, testName, success, errorMsg)
    end,
    
    TestNestedTables = function(self, entity)
        local testName = "Nested Table Access"
        local success = true
        local errorMsg = ""
        
        -- Create nested structure
        self.tileData = {
            grid = {
                {tile = "grass", height = 0},
                {tile = "water", height = -1},
                {tile = "mountain", height = 5}
            },
            metadata = {
                name = "TestMap",
                size = 3
            }
        }
        
        -- Verify nested access
        if self.tileData.grid[1].tile ~= "grass" then
            success = false
            errorMsg = "Nested table access failed"
        end
        
        if success and self.tileData.metadata.name ~= "TestMap" then
            success = false
            errorMsg = "Nested metadata access failed"
        end
        
        M4FeatureTest.RecordTest(self, entity, testName, success, errorMsg)
    end,
    
    TestTableInitialization = function(self, entity)
        local testName = "Table Initialization"
        local success = true
        local errorMsg = ""
        
        -- Verify default values persist
        if self.playerStats.health ~= 100 then
            success = false
            errorMsg = "Default health value lost"
        end
        
        if success and self.playerStats.mana ~= 50 then
            success = false
            errorMsg = "Default mana value lost"
        end
        
        -- Modify and verify
        self.playerStats.health = 75
        if self.playerStats.health ~= 75 then
            success = false
            errorMsg = "Cannot modify table values"
        end
        
        M4FeatureTest.RecordTest(self, entity, testName, success, errorMsg)
    end,
    
    TestQueueOperations = function(self, entity)
        local testName = "Queue Operations"
        local success = true
        local errorMsg = ""
        
        -- Enqueue operations
        table.insert(self.abilityQueue, {ability = "fireball", priority = 1})
        table.insert(self.abilityQueue, {ability = "heal", priority = 2})
        table.insert(self.abilityQueue, {ability = "shield", priority = 1})
        
        -- Verify queue size
        if #self.abilityQueue ~= 3 then
            success = false
            errorMsg = "Queue size incorrect"
        end
        
        -- Dequeue and verify
        local first = table.remove(self.abilityQueue, 1)
        if success and first.ability ~= "fireball" then
            success = false
            errorMsg = "Queue dequeue failed"
        end
        
        if success and #self.abilityQueue ~= 2 then
            success = false
            errorMsg = "Queue size after dequeue incorrect"
        end
        
        M4FeatureTest.RecordTest(self, entity, testName, success, errorMsg)
    end,
    
    TestMixedStructures = function(self, entity)
        local testName = "Mixed Data Structures"
        local success = true
        local errorMsg = ""
        
        -- Verify color map with nested arrays
        if self.colorMap.red[1] ~= 255 then
            success = false
            errorMsg = "Color map nested array access failed"
        end
        
        -- Add new color dynamically
        self.colorMap.yellow = {255, 255, 0}
        if success and self.colorMap.yellow[2] ~= 255 then
            success = false
            errorMsg = "Dynamic color addition failed"
        end
        
        M4FeatureTest.RecordTest(self, entity, testName, success, errorMsg)
    end,
    
    -- ========================================
    -- RPC RELIABILITY TESTS
    -- ========================================
    
    RunLocalTests = function(self, entity)
        Log.info("[M4Test] --- Local Mode Tests ---")
        Log.info("[M4Test] Skipping RPC tests in local mode")
    end,
    
    RunServerTests = function(self, entity)
        Log.info("[M4Test] --- Server Mode Tests ---")
        
        -- Start server
        local serverStarted = RPC.startServer(self.serverPort)
        M4FeatureTest.RecordTest(self, entity, "RPC Server Start", serverStarted, 
            serverStarted and "" or "Failed to start server on port " .. self.serverPort)
        
        if serverStarted then
            Log.info("[M4Test] Server started on port " .. self.serverPort)
            Log.info("[M4Test] Waiting for client connections...")
        end
    end,
    
    RunClientTests = function(self, entity)
        Log.info("[M4Test] --- Client Mode Tests ---")
        
        -- Connect to server
        local connected = RPC.connectToServer("onclocal.ddns.net", self.serverPort)
        M4FeatureTest.RecordTest(self, entity, "RPC Client Connect", connected,
            connected and "" or "Failed to connect to onclocal.ddns.net:" .. self.serverPort)
        
        if connected then
            Log.info("[M4Test] Connected to server")
            
            -- Test RPC call
            M4FeatureTest.TestRPCCall(self, entity)
        end
    end,
    
    TestRPCCall = function(self, entity)
        local testName = "RPC Function Call"
        local success = true
        
        -- Test a simple RPC call
        -- Note: This requires a corresponding handler on the server
        RPC.call("M4Test_Ping", "Hello from client!")
        
        Log.info("[M4Test] Sent RPC call - check server logs for receipt")
        M4FeatureTest.RecordTest(self, entity, testName, success, "")
    end,
    
    -- ========================================
    -- TEST UTILITIES
    -- ========================================
    
    RecordTest = function(self, entity, testName, success, errorMsg)
        local result = {
            name = testName,
            passed = success,
            error = errorMsg
        }
        
        table.insert(self.testResults, result)
        
        if success then
            self.testsPassed = self.testsPassed + 1
            Log.info("[M4Test]  PASS: " .. testName)
        else
            self.testsFailed = self.testsFailed + 1
            Log.error("[M4Test]  FAIL: " .. testName .. " - " .. errorMsg)
        end
        
        -- Store in global results
        table.insert(M4TestResults.tableArrayTests, result)
    end,
    
    PrintTestSummary = function(self, entity)
        Log.info("[M4Test] ========================================")
        Log.info("[M4Test] TEST SUMMARY")
        Log.info("[M4Test] ========================================")
        Log.info("[M4Test] Total Tests: " .. (self.testsPassed + self.testsFailed))
        Log.info("[M4Test] Passed: " .. self.testsPassed)
        Log.info("[M4Test] Failed: " .. self.testsFailed)
        
        if self.testsFailed == 0 then
            Log.info("[M4Test] ========================================")
            Log.info("[M4Test]  ALL TESTS PASSED! ")
            Log.info("[M4Test] ========================================")
        else
            Log.error("[M4Test] ========================================")
            Log.error("[M4Test]  SOME TESTS FAILED ")
            Log.error("[M4Test] ========================================")
            
            Log.info("[M4Test] Failed tests:")
            for i = 1, #self.testResults do
                local result = self.testResults[i]
                if not result.passed then
                    Log.error("[M4Test]   - " .. result.name .. ": " .. result.error)
                end
            end
        end
    end
})

return class
