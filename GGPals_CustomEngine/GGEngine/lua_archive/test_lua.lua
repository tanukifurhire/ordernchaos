-- Test Lua ECS integration
print("Lua script loaded successfully!")

-- Create entity
local player = ECS.createEntity()
print("Created entity:", player)

-- Add transform component
ECS.addTransform(player, 100, 200, 1, 1, 0)

-- Get transform
local transform = ECS.getTransform(player)
print("Position:", transform.x, transform.y)

-- Modify transform
transform.x = transform.x + 50
ECS.setTransform(player, transform)

-- Verify change
local newTransform = ECS.getTransform(player)
print("New position:", newTransform.x, newTransform.y)

-- Add rigidbody
ECS.addRigidbody(player, 10, 5)

-- Get rigidbody
local rb = ECS.getRigidbody(player)
print("Velocity:", rb.vx, rb.vy)

-- Check components
if ECS.hasComponent(player, "Transform2DFloat") then
    print("Entity has transform component")
end

print("Lua test completed successfully!")