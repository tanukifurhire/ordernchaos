/*************************************************************************
  \file       GGEvent.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       3 October 2025
  \version    1.0
  \brief
    Defines observer pattern event system with type-safe callback registration.
  \details
    This file implements GGEvent<Args...>, a variadic template class for
    publish-subscribe event dispatch. Supports arbitrary callback signatures
    via perfect forwarding and std::function type erasure. The system enables
    decoupled communication between ECS systems, UI components, and gameplay
    logic without tight coupling via direct method calls. Callbacks are
    invoked synchronously in registration order, enabling deterministic
    event propagation for debugging and replay systems.
    
    Design Characteristics:
    - Type-safe: Compile-time verification of callback signatures
    - Flexible: Supports lambdas, function pointers, member functions via bind
    - Zero-cost when unused: Empty event objects have minimal overhead
    - Deterministic: Callbacks invoked in FIFO registration order
    
    Limitations:
    - No priority system (callbacks execute in registration order)
    - Synchronous dispatch (no event queueing or deferred execution)
    - Removal by type match is fragile (multiple lambdas share type_index), but
      mitigated through requiring lvalue references in callback registration.
  \dependencies
    <functional>, <vector>
  \see
    Callback, SystemInterface, World
  \history
    - 1.0 (3 October 2025): Initial implementation with operator overloads.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <functional>
#include <vector>

/*************************************************************************
  \typedef Callback
  \brief
    Type alias for std::function callback with reference parameters.
  \tparam Args
    Variadic parameter types for callback signature.
  \details
    Uses non-const references (Args&...) to enable callbacks to modify
    event arguments. This design supports event filtering patterns where
    a callback can mark an event as "handled" to short-circuit propagation.
  \see
    GGEvent::Emit, std::function
*************************************************************************/
template<typename... Args>
using Callback = std::function<void(Args&...)>;

/*************************************************************************
  \class GGEvent
  \brief
    Observer pattern event dispatcher with type-safe callback management.
  \tparam Args
    Variadic parameter types for event payload.
  \details
    Maintains a dynamic vector of std::function callbacks invoked when
    Emit() is called. Supports registration via AddCallback/operator+=
    and removal via RemoveCallback/operator-=. The class is copyable
    (std::vector semantics) but callbacks are not equality-comparable,
    making removal-by-value impossible. Instead, removal uses
    std::function::target_type() to match callback types.
  \see
    Callback, operator+=, Emit
*************************************************************************/
template<typename... Args>
class GGEvent {
public:
	GGEvent() {};
/*************************************************************************
  \brief
    Invokes all registered callbacks with perfect-forwarded arguments.
  \param[in,out] args
    Event payload forwarded to each callback (mutable references).
  \details
    Iterates callback_list in registration order, invoking each non-null
    std::function. Arguments are forwarded via std::forward to preserve
    value category (lvalue vs rvalue). Callbacks receive references to
    the same argument instances, enabling inter-callback communication
    via argument mutation.
  \par Usage
  \code
    GGEvent<int, float> onDamage;
    onDamage += [](int& amount, float& mult) { amount *= mult; mult = 1.0f; };
    
    int damage = 50;
    float multiplier = 1.5f;
    onDamage.Emit(damage, multiplier);
    // damage == 75, multiplier == 1.0f
  \endcode
*************************************************************************/
	void Emit(Args&&... args) {
		for (const Callback<Args...>& callback : callback_list) {
			if (callback) {
				callback(args...);
			}
		}
	}
/*************************************************************************
  \brief
    Registers callback for future event dispatch.
  \param[in] callback
    std::function to invoke on Emit (null callbacks are ignored).
  \details
    Appends callback to callback_list via std::vector::push_back. Null
    callbacks (default-constructed std::function) are silently ignored
    to prevent null dereference during Emit. Order of registration is
    preserved (FIFO dispatch).
  \par Usage
  \code
    GGEvent<Entity> onEntitySpawn;
    onEntitySpawn.AddCallback([](Entity& e) {
        std::cout << "Entity " << e.id << " spawned\n";
    });
  \endcode
*************************************************************************/
	void AddCallback(Callback<Args...>& callback) {
		if (callback) {
			callback_list.push_back(callback);
		}
	}
/*************************************************************************
  \brief
    Unregisters callbacks matching type signature.
  \param[in] callback
    Template callback used for type matching via target_type().
  \details
    Removes all callbacks whose target_type() matches the provided callback's
    type. Uses std::remove_if + erase idiom for O(N) removal. Type matching
    is performed via std::type_index comparison, which works for stateless
    lambdas and function pointers but may match multiple callbacks with
    identical signatures.
*************************************************************************/
	void RemoveCallback(Callback<Args...>& callback) {
		callback_list.erase(
			std::remove_if(callback_list.begin(), callback_list.end(),
				[&callback](const Callback<Args...>& child) {
                    return child.target_type() == callback.target_type();
                }),
			    callback_list.end());
	}
/*************************************************************************
  \brief
    Operator overload for callback registration (syntactic sugar).
  \param[in] callback
    Callback to register.
  \return
    Reference to this event for chaining.
  \details
    Equivalent to AddCallback but enables fluent interface:
    ```cpp
    event += callback1;
    event += callback2;
    ```
  \see
    AddCallback
*************************************************************************/
	GGEvent& operator+=(Callback<Args...>& callback) {
		if (callback) {
			callback_list.push_back(callback);
		}

		return *this;
	}
/*************************************************************************
  \brief
    Operator overload for callback removal (syntactic sugar).
  \param[in] callback
    Callback to unregister (type-matched).
  \return
    Reference to this event for chaining.
  \details
    Equivalent to RemoveCallback. Removes callbacks via type matching
    using target_type() comparison.
  \see
    RemoveCallback
*************************************************************************/
	GGEvent& operator-=(Callback<Args...>& callback) {
        callback_list.erase(
            std::remove_if(callback_list.begin(), callback_list.end(),
                [&callback](const Callback<Args...>& child) {
                    return child.target_type() == callback.target_type();
                }),
                callback_list.end());

		return *this;
	}

protected:
	std::vector<Callback<Args...>> callback_list;
};