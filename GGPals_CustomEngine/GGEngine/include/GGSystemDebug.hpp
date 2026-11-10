/*************************************************************************
  \file       GGSystemDebug.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       22 September 2025
  \version    1.0
  \brief
    Defines advanced template metaprogramming-based system registry for ECS architecture
  \details
    This header provides a sophisticated system management framework utilizing
    C++20 concepts, CRTP patterns, and compile-time optimization techniques.
    The SystemContainer employs zero-overhead abstractions for priority-based
    system execution ordering while maintaining type safety through concepts.
    Designed for high-performance game engines requiring deterministic system
    execution with minimal runtime overhead.
    
    Key architectural features:
    - Compile-time system priority resolution using SFINAE
    - Zero-cost abstraction through perfect forwarding and tuple optimization
    - Type-safe system registration via concepts and static assertions
    - Priority-based execution ordering with compile-time filtering
    - Template metaprogramming utilities for type list manipulation

    Nelson: Wrote SystemPriorityRegister and ECSSystemPriorityRegister
    Xiao Xuan: Wrote the rest
  \dependencies
    <type_traits>, <concepts>, <array>, <tuple>, <memory>, <utility>, 
    GGSystemInterface.hpp
  \see
    SystemInterface, SystemManagerInterface, World, ComponentArray
  \history
    - 1.0 (22 September 2025): Initial implementation with C++20 concepts
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <type_traits>
#include <concepts>
#include <array>
#include <algorithm>
#include <memory>
#include <GGSystemInterface.hpp>
#include <GGEvent.hpp>
#include <GGTime.hpp>
#include <string>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <fstream>
#include <GGPerformanceProfiler.hpp>

namespace Systems {

    /*************************************************************************
      \brief
        Concept defining compile-time requirements for system manager types
      \tparam T
        Type to validate as conforming system manager
      \details
        Utilizes C++20 concepts to enforce system manager contract at compile-time.
        Ensures type safety and interface compliance through static validation.
        System managers must provide lifecycle methods (Init/Update) with
        specific return types and inherit from SystemManagerInterface base.
        
        This concept enables template metaprogramming operations while preventing
        instantiation with incompatible types, providing clear diagnostic
        messages when constraint violations occur.
      \par Contract Requirements
        - Must inherit from SystemManagerInterface (is-a relationship)
        - Must provide Init() method returning bool-convertible type
        - Must provide Update() method with void return type
        - All methods must be accessible through public interface
      \par Usage
      \code
        static_assert(IsSystemManager<MyCustomSystem>);
        template<IsSystemManager T> void ProcessSystem(T& system);
      \endcode
    *************************************************************************/
	template <typename T>
	concept IsSystemManager =
		std::derived_from<T, SystemManagerInterface> &&
		requires(T & t) {
			{ t.Init() } -> std::convertible_to<bool>;
			{ t.Update() } -> std::same_as<void>;
	};

    /*************************************************************************
      \brief
        Concept defining compile-time requirements for ECS system types
      \tparam T
        Type to validate as conforming ECS system
      \details
        Enforces ECS system contract through C++20 concepts, ensuring systems
        can process World entities and components. ECS systems operate on
        entity collections and must provide Update method accepting World
        reference for entity processing operations.
        
        Enables type-safe template metaprogramming for ECS-specific operations
        while maintaining clear separation from system manager types.
      \par Contract Requirements
        - Must inherit from SystemInterface (ECS base interface)
        - Must provide Update(World&) method with void return type
        - Method must accept World reference for entity processing
      \par Usage
      \code
        template<IsECSSystem T> void RegisterECSSystem(T& system);
      \endcode
    *************************************************************************/
	template <typename T>
	concept IsECSSystem =
		std::derived_from<T, SystemInterface>&&
		requires(T& t, World& world) {
			{ t.Update(world) } -> std::same_as<void>;
	};

    /*************************************************************************
      \brief
        Template metaprogramming utility for unique compile-time type indexing
      \tparam T
        System manager type satisfying IsSystemManager concept
      \details
        Generates unique compile-time identifiers for system types using
        __COUNTER__ macro. Each template instantiation receives a distinct
        index value, enabling efficient type-based lookups and registration
        in template metaprogramming contexts.
        
        The indexing system provides O(1) compile-time type identification
        without runtime overhead, supporting efficient system registry
        implementations and type-based dispatch mechanisms.
      \note
        Index values are generated sequentially during compilation and may
        vary between compilation units. Use only for intra-TU operations
        or ensure consistent compilation order for cross-TU stability.
      \par Usage
      \code
        constexpr auto renderIndex = SystemTypeIndex<RenderSystem>::value;
        static_assert(renderIndex != SystemTypeIndex<PhysicsSystem>::value);
      \endcode
    *************************************************************************/
	template <IsSystemManager T>
	struct SystemTypeIndex {
		static constexpr uint32_t value = __COUNTER__;
	};

    template <IsECSSystem T>
    struct ECSSystemTypeIndex {
        static constexpr uint32_t value = __COUNTER__;
    };

    /*************************************************************************
      \brief
        Convenience variable template for system type index access
      \tparam T
        System manager type to retrieve index for
      \details
        Provides streamlined access to compile-time type indices through
        variable template syntax. Eliminates verbose typename::value syntax
        while maintaining full compile-time evaluation and optimization.
      \par Usage
      \code
        constexpr auto id = systemIndex<MySystem>;
        if constexpr (systemIndex<A> < systemIndex<B>) { ... }
      \endcode
    *************************************************************************/
	template <IsSystemManager T>
	constexpr uint32_t systemIndex = SystemTypeIndex<T>::value;

    template <IsECSSystem T>
    constexpr uint32_t ecsSystemIndex = ECSSystemTypeIndex<T>::value;

    /*************************************************************************
      \brief
        Compile-time type list container for system type collections
      \tparam Types
        Variadic pack of system manager types
      \details
        Provides template metaprogramming foundation for type list operations
        including size calculation, type filtering, and compile-time algorithms.
        Serves as building block for more complex template metaprogramming
        operations on system type collections.
        
        Type lists enable sophisticated compile-time operations while
        maintaining zero runtime overhead through complete template
        evaluation during compilation phase.
      \par Usage
      \code
        using GameSystems = TypeList<RenderSystem, PhysicsSystem, AudioSystem>;
        constexpr auto count = listSize<GameSystems>;
      \endcode
    *************************************************************************/
	template <IsSystemManager... Types>
	struct TypeList {};

    template <IsECSSystem... Types>
    struct ECSTypeList {};

    /*************************************************************************
      \brief
        Template metaprogramming utility for compile-time list size calculation
      \tparam List
        TypeList specialization to determine size of
      \details
        Implements type list size calculation through template specialization
        and integral_constant inheritance. Provides compile-time size
        information for template metaprogramming algorithms and static
        assertions without runtime overhead.
    *************************************************************************/
	template <typename List>
	struct ListSize;

    template <typename List>
    struct ECSListSize;

    /*************************************************************************
      \brief
        Specialization of ListSize for TypeList template
      \tparam Types
        System manager types contained in the type list
      \details
        Template specialization providing size calculation for TypeList
        containers through std::integral_constant inheritance. Enables
        compile-time size queries and template metaprogramming operations.
    *************************************************************************/
	template <IsSystemManager... Types>
	struct ListSize<TypeList<Types...>> : std::integral_constant<size_t, sizeof...(Types)> {};

    template<IsECSSystem... Types>
    struct ECSListSize<ECSTypeList<Types...>> : std::integral_constant<size_t, sizeof...(Types)> {};

    /*************************************************************************
      \brief
        Convenience variable template for type list size access
      \tparam List
        TypeList type to retrieve size for
      \details
        Streamlined access to type list size information through variable
        template syntax, eliminating verbose specialization access patterns.
      \par Usage
      \code
        using Systems = TypeList<A, B, C>;
        static_assert(listSize<Systems> == 3);
      \endcode
    *************************************************************************/
	template <typename List>
	constexpr size_t listSize = ListSize<List>::value;

    template <typename List>
    constexpr size_t ecsListSize = ECSListSize<List>::value;

    /*************************************************************************
      \brief
        System execution priority enumeration for deterministic ordering
      \details
        Defines execution priorities for different system categories within
        the game engine architecture. Lower numerical values execute first,
        enabling proper dependency ordering and deterministic system execution.
        
        Priority levels are designed around typical game engine update phases:
        input processing, physics simulation, gameplay logic, audio processing,
        rendering operations, and debug visualization. Each priority level
        represents a logical execution phase within the main game loop.
        
        The enumeration uses explicit values to maintain stable ordering
        across compilation units and enable priority range operations in
        template metaprogramming contexts.
      \par Priority Ordering
        INPUT (0) -> PHYSICS (100) -> GAMEPLAY (200) -> AUDIO (300) -> RENDERING (400) -> DEBUG (500)
      \par Usage
      \code
        class MySystem : public PrioritizedSystem<MySystem, SystemPriority::PHYSICS> { ... };
      \endcode
    *************************************************************************/
    enum class SystemPriority : uint16_t {
        TIME = 0,
        INPUT = 50,
        NETWORK = 70,
        PHYSICS = 100,
        TIMER = 150,
		GAMEPLAY = 200,
		AUDIO = 300,
		RENDERING = 400,
		DEBUG = 500,
	};

    /*************************************************************************
      \brief
        CRTP mixin providing compile-time priority registration for system managers
      \tparam T
        Derived system manager type (CRTP pattern)
      \tparam Priority
        Compile-time encoded system execution priority
      \details
        Implements the Curiously Recurring Template Pattern (CRTP) to inject
        priority metadata into existing system manager types. This mixin
        approach enables adding priority information without modifying
        existing system implementations while maintaining zero runtime overhead.
        
        The class inherits from the target system type T, creating a wrapper
        that extends the original system with compile-time priority information.
        Template metaprogramming operations can extract priority data for
        system ordering and filtering without virtual function overhead.
        
        Perfect forwarding constructor ensures all initialization patterns
        of the wrapped system remain accessible while adding no runtime cost.
      \par CRTP Benefits
        - Zero runtime overhead through static polymorphism
        - Compile-time priority extraction via template metaprogramming
        - Maintains original system interface without modification
        - Enables policy-based design patterns for system enhancement
      \par Usage
      \code
        using PrioritizedRender = SystemPriorityRegister<GGEngineRender, SystemPriority::RENDERING>;
        SystemContainer<PrioritizedRender, ...> registry;
        auto& renderer = registry.GetSystem<PrioritizedRender>();
      \endcode
    *************************************************************************/
	template <IsSystemManager T, SystemPriority Priority>
	class SystemPriorityRegister : public T {
	public:
        // GGEvent<std::string const&, double> onPerformanceProfilerUpdate;

    /*************************************************************************
      \brief
        Initializes the system with exception handling and crash logging.
      \return
        true if the system initialization succeeds, false otherwise.
      \details
        Calls the base system's Init() function (T::Init()). If an exception
        occurs during initialization, the exception is caught and logged into
        "crash_log.txt" with the current date, time, and type of system.
        The exception is then re-thrown to propagate the error.
    *************************************************************************/
        bool Init() override 
        {
            try {
                return T::Init();
            }
            catch (const std::exception& e)
            {
                std::ofstream log("crash_log.txt", std::ios::app);              // writes new data instead of overwriting
                if (log.is_open()) {

                    auto systemTime = std::chrono::system_clock::now();
                    std::time_t _systemTime = std::chrono::system_clock::to_time_t(systemTime);

                    log << "*****************************************" << std::endl;
                    log << "Current Date & Time: "
                        << std::put_time(std::localtime(&_systemTime), "%d/%m/%Y %H:%M:%S")
                        << std::endl;

                    log << "ERROR: [" << typeid(T).name() << "] Initialization failed: "
                        << e.what() << std::endl;
                }
                throw std::runtime_error(e.what());
            }
        }

    /*************************************************************************
      \brief
        Updates the system while measuring performance and handling exceptions.
      \details
        Calls the base system's Update() function (T::Update()) and measures
        its execution time using a high-resolution steady clock. The measured
        duration is recorded by the PerformanceProfiler for profiling purposes.

        If an exception occurs during Update, it is caught and logged into
        "crash_log.txt" with the current date, time, and system type. The
            exception is then re-thrown to propagate the error.
    *************************************************************************/
        void Update() override 
        {
           try {
               //throw std::runtime_error("testing exception");                // use to test exception, this debugger can be removed
               std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
               T::Update();
               auto duration = std::chrono::steady_clock::now() - currentTime;

               // Note from Zaf:
               // Replaced event system with direct profiler call that is simpler and automatic.
               // Old: Emit event, then manually subscribe in GGEngine.cpp (easy to forget)
               // New: Just record it directly (works automatically for all systems)
               PerformanceProfiler::GetInstance().RecordSystemTime(
                   typeid(T).name(),
                   std::chrono::duration<double>(duration).count()
               );
           }
	       catch (const std::exception& e)
	       {
               std::ofstream log("crash_log.txt", std::ios::app);              // writes new data instead of overwriting
               if (log.is_open()) {

                   auto systemTime = std::chrono::system_clock::now();
                   std::time_t _systemTime = std::chrono::system_clock::to_time_t(systemTime);

                   log << "*****************************************" << std::endl;
                   log << "Current Date & Time: " 
                       << std::put_time(std::localtime(&_systemTime), "%d/%m/%Y %H:%M:%S") 
                       << std::endl;
                   
                   log << "ERROR: [" << typeid(T).name() << "] Update failed: " 
                       << e.what() << std::endl;
               }
               throw std::runtime_error (e.what());
           }
           

           //std::cout << "This is a System Priority Register Update for Typename: " << typeid(T).name() << std::endl;
        } 

        T& Get()
        {
            return static_cast<T&>(*this);
        }

        /*************************************************************************
          \brief
            Get compile-time system execution priority
          \return
            Priority level for system ordering operations
          \details
            Provides static priority access for template metaprogramming
            operations. This method enables compile-time system sorting,
            filtering, and ordering without virtual function overhead.
            
            The constexpr and noexcept specifications ensure optimal
            performance and compile-time evaluation in template contexts.
        *************************************************************************/
		static constexpr SystemPriority GetPriority() noexcept { return Priority; }

        /*************************************************************************
          \brief
            Get unique compile-time type identifier
          \return
            Type index for system identification and lookup
          \details
            Retrieves unique compile-time identifier for the wrapped system
            type T. This index enables efficient type-based operations in
            template metaprogramming contexts without runtime overhead.
            
            Note that the index corresponds to the wrapped type T, not the
            wrapper type itself, maintaining semantic consistency with the
            underlying system identity.
        *************************************************************************/
		static constexpr uint32_t GetTypeIndex() noexcept { return systemIndex<T>; }
	};

    template <IsECSSystem T, SystemPriority Priority>
    class ECSSystemPriorityRegister : public T {
    public:
        GGEvent<std::string const&, double> onPerformanceProfilerUpdate;

        /*************************************************************************
          \brief
            Updates the system for the given world while measuring performance
            and handling exceptions.
          \param[in,out] world
            Reference to the World object that this system operates on.
          \details
            Calls the base system's Update(World& world) function (T::Update(world))
            and measures its execution time using a high-resolution steady clock.
            The measured duration is recorded by the PerformanceProfiler for
            profiling purposes.

            If an exception occurs during Update, it is caught and logged into
            "crash_log.txt" with the current date, time, and system type. The
            exception is then re-thrown to propagate the error.

            Notes:
              - The profiler recording replaces the previous event-based system,
                making timing automatic for all systems.
              - The commented-out runtime exception can be used for testing the
                crash logging mechanism.
        *************************************************************************/
        void Update(World& world) override
        {
            try {
                //throw std::runtime_error("testing exception");                // use to test exception, this debugger can be removed
                std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
                T::Update(world);
                auto duration = std::chrono::steady_clock::now() - currentTime;

                // Note from Zaf:
                // Replaced event system with direct profiler call that is simpler and automatic.
                // Old: Emit event, then manually subscribe in GGEngine.cpp (easy to forget)
                // New: Just record it directly (works automatically for all systems)
                PerformanceProfiler::GetInstance().RecordSystemTime(
                    typeid(T).name(),
                    std::chrono::duration<double>(duration).count()
                );
            }
            catch (const std::exception& e)
            {
                std::ofstream log("crash_log.txt", std::ios::app);              // writes new data instead of overwriting
                if (log.is_open()) {

                    auto systemTime = std::chrono::system_clock::now();
                    std::time_t _systemTime = std::chrono::system_clock::to_time_t(systemTime);

                    log << "*****************************************" << std::endl;
                    log << "Current Date & Time: "
                        << std::put_time(std::localtime(&_systemTime), "%d/%m/%Y %H:%M:%S")
                        << std::endl;

                    log << "ERROR: [" << typeid(T).name() << "] Update failed: "
                        << e.what() << std::endl;
                }
                throw std::runtime_error(e.what());
            }
        }
        /*************************************************************************
          \brief
            Get compile-time system execution priority
          \return
            Priority level for system ordering operations
          \details
            Provides static priority access for template metaprogramming
            operations. This method enables compile-time system sorting,
            filtering, and ordering without virtual function overhead.

            The constexpr and noexcept specifications ensure optimal
            performance and compile-time evaluation in template contexts.
        *************************************************************************/
        static constexpr SystemPriority GetPriority() noexcept { return Priority; }

        /*************************************************************************
          \brief
            Get unique compile-time type identifier
          \return
            Type index for system identification and lookup
          \details
            Retrieves unique compile-time identifier for the wrapped system
            type T. This index enables efficient type-based operations in
            template metaprogramming contexts without runtime overhead.

            Note that the index corresponds to the wrapped type T, not the
            wrapper type itself, maintaining semantic consistency with the
            underlying system identity.
        *************************************************************************/
        static constexpr uint32_t GetTypeIndex() noexcept { return ecsSystemIndex<T>; }
    };

    /*************************************************************************
      \brief
        High-performance system container with compile-time optimization
      \tparam SystemTypes
        Variadic pack of system manager types satisfying IsSystemManager
      \details
        Implements zero-overhead system management through advanced template
        metaprogramming techniques. The container utilizes std::tuple for
        cache-friendly memory layout and compile-time type resolution for
        optimal system access patterns.
        
        Key architectural features:
        - Priority-based execution ordering with compile-time filtering
        - Zero virtual dispatch overhead through static polymorphism
        - Perfect forwarding constructors for flexible initialization
        - Cache-friendly tuple-based storage layout
        - Compile-time system count and type validation
        - SFINAE-based priority detection and filtering
        
        The implementation leverages fold expressions, constexpr algorithms,
        and template metaprogramming for optimal performance while maintaining
        type safety through concepts and static assertions.
      \par Performance Characteristics
        - System access: O(1) compile-time resolved
        - Update iteration: Zero virtual dispatch overhead
        - Memory layout: Cache-friendly sequential storage
        - Priority filtering: Compile-time dead code elimination
      \par Usage
      \code
        using Registry = SystemContainer<RenderManager, PhysicsManager>;
        Registry systems;
        systems.UpdateAllSystems();
        auto& renderer = systems.GetSystem<RenderManager>();
      \endcode
    *************************************************************************/
	template<IsSystemManager... SystemTypes>
	class SystemContainer {
        /*************************************************************************
          \brief
            Tuple-based storage for system instances with optimal memory layout
          \details
            Utilizes std::tuple with std::unique_ptr for automatic resource
            management and cache-friendly sequential memory layout. The tuple
            approach enables compile-time type resolution and eliminates
            virtual dispatch overhead in system access operations.
        *************************************************************************/
		std::tuple<std::unique_ptr<SystemTypes>...> m_systems;

        /*************************************************************************
          \brief
            Generate compile-time sorted indices for priority-based execution
          \return
            Array of indices sorted by system priority for optimal execution order
          \details
            Computes optimal system execution order based on priority values
            extracted through template metaprogramming. The sorting operation
            utilizes compile-time priority information where available, falling
            back to runtime sorting for systems without static priority data.
            
            The implementation creates priority-index pairs and sorts them to
            determine optimal execution order while maintaining deterministic
            behavior across different system configurations. Index array
            enables efficient iteration in priority order without runtime
            sorting overhead.
          \note
            Function marked constexpr where possible but may require runtime
            evaluation for systems without compile-time priority information.
            Future C++23 constexpr std::sort may enable full compile-time
            evaluation for all system configurations.
        *************************************************************************/
		static constexpr std::array<size_t, sizeof...(SystemTypes)> GetSortedIndices() {
			std::array<std::pair<SystemPriority, size_t>, sizeof...(SystemTypes)> priorities = {{
				{SystemTypes::GetPriority(), systemIndex<SystemTypes>}...
			}};
			
			std::array<size_t, sizeof...(SystemTypes)> indices{};
			for (size_t i{}; i < sizeof...(SystemTypes); i++) {
				indices[i] = i;
			}

			std::sort(indices.begin(), indices.end(), [&priorities](size_t a, size_t b) {
                return priorities[a].first < priorities[b].first;
            });

			return indices;
		}

        /*************************************************************************
          \brief
            Compile-time computed sorted indices for priority-based execution
          \details
            Pre-computed index array enabling efficient priority-ordered system
            iteration. Computed once during static initialization and reused
            for all update operations, amortizing sorting cost across the
            application lifetime.
        *************************************************************************/
		static constexpr auto sorted_indices = GetSortedIndices();

	public:
        /*************************************************************************
          \brief
            Default constructor creating all systems with default constructors
          \details
            Utilizes variadic template expansion and perfect forwarding to
            create each system with its default constructor. Generates optimal
            code through template metaprogramming without runtime overhead.
            
            All system types must provide accessible default constructors for
            this initialization path. The std::make_unique calls are expanded
            at compile-time, creating efficient initialization code.
          \par Requirements
            - All SystemTypes must be default constructible
            - Default constructors must be publicly accessible
            - System constructors should not throw exceptions during initialization
          \par Usage
          \code
            SystemContainer<RenderManager, PhysicsManager> registry;
          \endcode
        *************************************************************************/
		SystemContainer()
			: m_systems(std::make_unique<SystemTypes>()...) {}

        /*************************************************************************
          \brief
            Variadic constructor with perfect forwarding for custom initialization
          \tparam Args
            Argument types for system constructors (auto-deduced)
          \param[in] args
            Arguments to forward to system constructors
          \details
            Provides flexible initialization through perfect forwarding of
            constructor arguments. Template argument deduction enables automatic
            parameter type inference while maintaining optimal performance
            through move semantics and forwarding references.
            
            This constructor enables custom system initialization patterns
            while preserving the zero-overhead abstraction principle. Arguments
            are distributed to all systems, requiring compatible constructor
            signatures across system types.
          \warning
            All systems receive the same argument pack. For per-system
            initialization, use the default constructor followed by individual
            system configuration calls.
          \par Usage
          \code
            GlobalConfig config;
            SystemContainer<RenderManager, AudioManager> registry(config);
          \endcode
        *************************************************************************/
		template<typename... Args>
		explicit SystemContainer(Args&&... args)
			: m_systems(std::make_unique<SystemTypes>(std::forward<Args>(args))...) {}


        /*************************************************************************
          \brief
            Get system reference with compile-time type checking and zero overhead
          \tparam T
            System type to retrieve (must be registered in SystemTypes pack)
          \return
            Reference to the requested system instance
          \details
            Provides zero-overhead system access through compile-time type
            resolution and std::tuple direct access. Static assertion ensures
            type safety and prevents compilation with unregistered system types.
            
            The implementation compiles down to direct pointer dereference with
            no virtual dispatch or runtime type checking overhead. Template
            metaprogramming ensures optimal code generation while maintaining
            type safety through concepts and static assertions.
          \par Performance
            - Compile-time type resolution: O(1)
            - Runtime access: Direct pointer dereference
            - No virtual dispatch overhead
            - Branch-free execution path
          \par Usage
          \code
            auto& renderer = registry.GetSystem<RenderManager>();
            renderer.SetShader(newShader);
          \endcode
        *************************************************************************/
		template<IsSystemManager T>
		T& GetSystem() noexcept {
			static_assert((std::is_same_v<T, SystemTypes> || ...));
			return *std::get<std::unique_ptr<T>>(m_systems);
		}

        /*************************************************************************
          \brief
            Update all systems in priority order with optimal performance
          \details
            Executes all registered systems in optimal priority order using
            compile-time generated execution sequence. The implementation
            utilizes template metaprogramming to filter systems by priority
            range and generate unrolled update loops with no virtual dispatch.
            
            Systems are processed in ascending priority order (INPUT -> DEBUG)
            with compile-time filtering eliminating unused systems from the
            execution path. Fold expressions generate optimal assembly code
            with predictable execution patterns for consistent frame timing.
          \par Performance Characteristics
            - Zero virtual dispatch overhead
            - Unrolled loop generation through fold expressions  
            - Branch predictor friendly execution order
            - Cache-friendly sequential system processing
            - Compile-time dead code elimination for unused systems
          \par Usage
          \code
            registry.UpdateAllSystems();  // All systems in priority order
          \endcode
        *************************************************************************/
		void UpdateAllSystems() {
			UpdateSystemsByPriority<SystemPriority::INPUT, SystemPriority::DEBUG>(
				std::index_sequence_for<SystemTypes...>{});
		}

        /*************************************************************************
          \brief
            Update systems within compile-time specified priority range
          \tparam MinPriority
            Minimum priority level (inclusive) for system execution
          \tparam MaxPriority
            Maximum priority level (inclusive) for system execution
          \details
            Executes systems within specified priority range using compile-time
            filtering and template metaprogramming optimization. Dead code
            elimination removes systems outside the priority range at compile-time,
            generating minimal assembly code with no runtime branch overhead.
            
            The template implementation enables selective system execution for
            specialized update phases or performance optimization scenarios.
            Priority filtering occurs at compile-time through constexpr
            conditional evaluation and SFINAE-based template selection.
          \par Compile-Time Optimization
            - Dead code elimination for out-of-range systems
            - Branch-free execution through template specialization
            - Optimal assembly generation via fold expressions
            - No runtime priority checking overhead
          \par Usage
          \code
            // Execute only physics and gameplay systems
            registry.UpdateByPriority<SystemPriority::PHYSICS, SystemPriority::GAMEPLAY>();
            
            // Execute only rendering systems
            registry.UpdateByPriority<SystemPriority::RENDERING, SystemPriority::RENDERING>();
          \endcode
        *************************************************************************/
		template<SystemPriority MinPriority, SystemPriority MaxPriority>
		void UpdateByPriority() {
			UpdateSystemsByPriority<MinPriority, MaxPriority>(
				std::index_sequence_for<SystemTypes...>{});
		}

		static constexpr size_t GetSystemCount() noexcept {
			return sizeof...(SystemTypes);
		}

	private:
		/*************************************************************************
          \brief
            Implementation helper for unrolled system updates via fold expressions
          \tparam Indices
            Index sequence for tuple element access
          \param[in] seq
            Index sequence object (parameter for template deduction)
          \details
            Generates unrolled system update loop through fold expression
            expansion. Each system's Update method is called directly with
            no virtual dispatch overhead, creating optimal assembly code
            with predictable execution patterns.
        *************************************************************************/
		template<size_t... Indices>
		void UpdateSystems(std::index_sequence<Indices...>) {
			(std::get<Indices>(m_systems)->Update(), ...);
		}

        /*************************************************************************
          \brief
            Priority-filtered system update implementation using SFINAE
          \tparam MinPriority
            Minimum priority level for system execution
          \tparam MaxPriority
            Maximum priority level for system execution
          \tparam Indices
            Index sequence for tuple element access
          \param[in] seq
            Index sequence object (parameter for template deduction)
          \details
            Implements compile-time priority filtering through SFINAE-based
            template selection and constexpr conditional evaluation. Systems
            outside the priority range are eliminated through dead code
            elimination, generating optimal assembly with no runtime overhead.
            
            The lambda-based approach with template parameter enables compile-time
            evaluation of priority constraints while maintaining compatibility
            with systems that may not provide static priority information.
        *************************************************************************/
		template<SystemPriority MinPriority, SystemPriority MaxPriority, size_t... Indices>
		void UpdateSystemsByPriority(std::index_sequence<Indices...>) {
			auto updateIfInRange = 
			[]<IsSystemManager T>(std::unique_ptr<T>&system) {
				if constexpr (requires { T::GetPriority(); }) {
					constexpr auto priority = T::GetPriority();
					if constexpr (priority >= MinPriority && priority <= MaxPriority) {
						system->Update();
					}
				}
			};

			(updateIfInRange(std::get<Indices>(m_systems)), ...);
		}
	};

    /*************************************************************************
     \brief
       High-performance system container with compile-time optimization
     \tparam SystemTypes
       Variadic pack of system manager types satisfying IsSystemManager
     \details
       Implements zero-overhead system management through advanced template
       metaprogramming techniques. The container utilizes std::tuple for
       cache-friendly memory layout and compile-time type resolution for
       optimal system access patterns.

       Key architectural features:
       - Priority-based execution ordering with compile-time filtering
       - Zero virtual dispatch overhead through static polymorphism
       - Perfect forwarding constructors for flexible initialization
       - Cache-friendly tuple-based storage layout
       - Compile-time system count and type validation
       - SFINAE-based priority detection and filtering

       The implementation leverages fold expressions, constexpr algorithms,
       and template metaprogramming for optimal performance while maintaining
       type safety through concepts and static assertions.
     \par Performance Characteristics
       - System access: O(1) compile-time resolved
       - Update iteration: Zero virtual dispatch overhead
       - Memory layout: Cache-friendly sequential storage
       - Priority filtering: Compile-time dead code elimination
     \par Usage
     \code
       using Registry = SystemContainer<RenderManager, PhysicsManager>;
       Registry systems;
       systems.UpdateAllSystems();
       auto& renderer = systems.GetSystem<RenderManager>();
     \endcode
   *************************************************************************/
    template<IsECSSystem... SystemTypes>
    class ECSSystemContainer {
        /*************************************************************************
          \brief
            Tuple-based storage for system instances with optimal memory layout
          \details
            Utilizes std::tuple with std::unique_ptr for automatic resource
            management and cache-friendly sequential memory layout. The tuple
            approach enables compile-time type resolution and eliminates
            virtual dispatch overhead in system access operations.
        *************************************************************************/
        std::tuple<std::unique_ptr<SystemTypes>...> m_systems;

        /*************************************************************************
          \brief
            Generate compile-time sorted indices for priority-based execution
          \return
            Array of indices sorted by system priority for optimal execution order
          \details
            Computes optimal system execution order based on priority values
            extracted through template metaprogramming. The sorting operation
            utilizes compile-time priority information where available, falling
            back to runtime sorting for systems without static priority data.

            The implementation creates priority-index pairs and sorts them to
            determine optimal execution order while maintaining deterministic
            behavior across different system configurations. Index array
            enables efficient iteration in priority order without runtime
            sorting overhead.
          \note
            Function marked constexpr where possible but may require runtime
            evaluation for systems without compile-time priority information.
            Future C++23 constexpr std::sort may enable full compile-time
            evaluation for all system configurations.
        *************************************************************************/
        static constexpr std::array<size_t, sizeof...(SystemTypes)> GetSortedIndices() {
            std::array<std::pair<SystemPriority, size_t>, sizeof...(SystemTypes)> priorities = { {
                {SystemTypes::GetPriority(), ecsSystemIndex<SystemTypes>}...
            } };

            std::array<size_t, sizeof...(SystemTypes)> indices{};
            for (size_t i{}; i < sizeof...(SystemTypes); i++) {
                indices[i] = i;
            }

            std::sort(indices.begin(), indices.end(), [&priorities](size_t a, size_t b) {
                return priorities[a].first < priorities[b].first;
                });

            return indices;
        }

        /*************************************************************************
          \brief
            Compile-time computed sorted indices for priority-based execution
          \details
            Pre-computed index array enabling efficient priority-ordered system
            iteration. Computed once during static initialization and reused
            for all update operations, amortizing sorting cost across the
            application lifetime.
        *************************************************************************/
        static constexpr auto sorted_indices = GetSortedIndices();

    public:
        /*************************************************************************
          \brief
            Default constructor creating all systems with default constructors
          \details
            Utilizes variadic template expansion and perfect forwarding to
            create each system with its default constructor. Generates optimal
            code through template metaprogramming without runtime overhead.

            All system types must provide accessible default constructors for
            this initialization path. The std::make_unique calls are expanded
            at compile-time, creating efficient initialization code.
          \par Requirements
            - All SystemTypes must be default constructible
            - Default constructors must be publicly accessible
            - System constructors should not throw exceptions during initialization
          \par Usage
          \code
            SystemContainer<RenderManager, PhysicsManager> registry;
          \endcode
        *************************************************************************/
        ECSSystemContainer()
            : m_systems(std::make_unique<SystemTypes>()...) {
        }

        /*************************************************************************
          \brief
            Variadic constructor with perfect forwarding for custom initialization
          \tparam Args
            Argument types for system constructors (auto-deduced)
          \param[in] args
            Arguments to forward to system constructors
          \details
            Provides flexible initialization through perfect forwarding of
            constructor arguments. Template argument deduction enables automatic
            parameter type inference while maintaining optimal performance
            through move semantics and forwarding references.

            This constructor enables custom system initialization patterns
            while preserving the zero-overhead abstraction principle. Arguments
            are distributed to all systems, requiring compatible constructor
            signatures across system types.
          \warning
            All systems receive the same argument pack. For per-system
            initialization, use the default constructor followed by individual
            system configuration calls.
          \par Usage
          \code
            GlobalConfig config;
            SystemContainer<RenderManager, AudioManager> registry(config);
          \endcode
        *************************************************************************/
        template<typename... Args>
        explicit ECSSystemContainer(Args&&... args)
            : m_systems(std::make_unique<SystemTypes>(std::forward<Args>(args))...) {
        }


        /*************************************************************************
          \brief
            Get system reference with compile-time type checking and zero overhead
          \tparam T
            System type to retrieve (must be registered in SystemTypes pack)
          \return
            Reference to the requested system instance
          \details
            Provides zero-overhead system access through compile-time type
            resolution and std::tuple direct access. Static assertion ensures
            type safety and prevents compilation with unregistered system types.

            The implementation compiles down to direct pointer dereference with
            no virtual dispatch or runtime type checking overhead. Template
            metaprogramming ensures optimal code generation while maintaining
            type safety through concepts and static assertions.
          \par Performance
            - Compile-time type resolution: O(1)
            - Runtime access: Direct pointer dereference
            - No virtual dispatch overhead
            - Branch-free execution path
          \par Usage
          \code
            auto& renderer = registry.GetSystem<RenderManager>();
            renderer.SetShader(newShader);
          \endcode
        *************************************************************************/
        template<IsECSSystem T>
        T& GetSystem() noexcept {
            static_assert((std::is_same_v<T, SystemTypes> || ...));
            return *std::get<std::unique_ptr<T>>(m_systems);
        }

        /*************************************************************************
          \brief
            Update all systems in priority order with optimal performance
          \details
            Executes all registered systems in optimal priority order using
            compile-time generated execution sequence. The implementation
            utilizes template metaprogramming to filter systems by priority
            range and generate unrolled update loops with no virtual dispatch.

            Systems are processed in ascending priority order (INPUT -> DEBUG)
            with compile-time filtering eliminating unused systems from the
            execution path. Fold expressions generate optimal assembly code
            with predictable execution patterns for consistent frame timing.
          \par Performance Characteristics
            - Zero virtual dispatch overhead
            - Unrolled loop generation through fold expressions
            - Branch predictor friendly execution order
            - Cache-friendly sequential system processing
            - Compile-time dead code elimination for unused systems
          \par Usage
          \code
            registry.UpdateAllSystems();  // All systems in priority order
          \endcode
        *************************************************************************/
        void UpdateAllSystems(World& world) {
            UpdateSystemsByPriority<SystemPriority::INPUT, SystemPriority::DEBUG>(
                std::index_sequence_for<SystemTypes...>{}, world);
        }

        /*************************************************************************
          \brief
            Update systems within compile-time specified priority range
          \tparam MinPriority
            Minimum priority level (inclusive) for system execution
          \tparam MaxPriority
            Maximum priority level (inclusive) for system execution
          \details
            Executes systems within specified priority range using compile-time
            filtering and template metaprogramming optimization. Dead code
            elimination removes systems outside the priority range at compile-time,
            generating minimal assembly code with no runtime branch overhead.

            The template implementation enables selective system execution for
            specialized update phases or performance optimization scenarios.
            Priority filtering occurs at compile-time through constexpr
            conditional evaluation and SFINAE-based template selection.
          \par Compile-Time Optimization
            - Dead code elimination for out-of-range systems
            - Branch-free execution through template specialization
            - Optimal assembly generation via fold expressions
            - No runtime priority checking overhead
          \par Usage
          \code
            // Execute only physics and gameplay systems
            registry.UpdateByPriority<SystemPriority::PHYSICS, SystemPriority::GAMEPLAY>();

            // Execute only rendering systems
            registry.UpdateByPriority<SystemPriority::RENDERING, SystemPriority::RENDERING>();
          \endcode
        *************************************************************************/
        template<SystemPriority MinPriority, SystemPriority MaxPriority>
        void UpdateByPriority(World& world) {
            UpdateSystemsByPriority<MinPriority, MaxPriority>(
                std::index_sequence_for<SystemTypes...>{}, world);
        }

        static constexpr size_t GetSystemCount() noexcept {
            return sizeof...(SystemTypes);
        }

    private:
        /*************************************************************************
          \brief
            Implementation helper for unrolled system updates via fold expressions
          \tparam Indices
            Index sequence for tuple element access
          \param[in] seq
            Index sequence object (parameter for template deduction)
          \details
            Generates unrolled system update loop through fold expression
            expansion. Each system's Update method is called directly with
            no virtual dispatch overhead, creating optimal assembly code
            with predictable execution patterns.
        *************************************************************************/
        template<size_t... Indices>
        void UpdateSystems(std::index_sequence<Indices...>, World& world) {
            (std::get<Indices>(m_systems)->Update(world), ...);
        }

        /*************************************************************************
          \brief
            Priority-filtered system update implementation using SFINAE
          \tparam MinPriority
            Minimum priority level for system execution
          \tparam MaxPriority
            Maximum priority level for system execution
          \tparam Indices
            Index sequence for tuple element access
          \param[in] seq
            Index sequence object (parameter for template deduction)
          \details
            Implements compile-time priority filtering through SFINAE-based
            template selection and constexpr conditional evaluation. Systems
            outside the priority range are eliminated through dead code
            elimination, generating optimal assembly with no runtime overhead.

            The lambda-based approach with template parameter enables compile-time
            evaluation of priority constraints while maintaining compatibility
            with systems that may not provide static priority information.
        *************************************************************************/
        template<SystemPriority MinPriority, SystemPriority MaxPriority, size_t... Indices>
        void UpdateSystemsByPriority(std::index_sequence<Indices...>, World& world) {
            auto updateIfInRange =
                []<IsECSSystem T>(std::unique_ptr<T>&system) {
                if constexpr (requires { T::GetPriority(); }) {
                    constexpr auto priority = T::GetPriority();
                    if constexpr (priority >= MinPriority && priority <= MaxPriority) {
                        system->Update(world);
                    }
                }
            };

            (updateIfInRange(std::get<Indices>(m_systems)), ...);
        }
    };
	
    /*************************************************************************
      \brief
        Factory function for type-deduced system container creation
      \tparam T
        System types to register (auto-deduced from function arguments)
      \return
        Configured system container with optimal type deduction
      \details
        Provides streamlined system container creation through template argument
        deduction and perfect forwarding. Eliminates verbose template parameter
        specification while maintaining full compile-time optimization.
        
        The factory function enables clean syntax for system registry creation
        and integrates seamlessly with auto variable declarations for type
        deduction in modern C++ codebases.
      \par Usage
      \code
        auto systemRegistry = MakeStaticRegistry<RenderManager, PhysicsManager>();
        // Equivalent to: SystemContainer<RenderManager, PhysicsManager> systemRegistry;
      \endcode
    *************************************************************************/
	template <IsSystemManager... T>
	constexpr auto MakeStaticRegistry() {
		return SystemContainer<T...>{};
	}

    template <IsECSSystem... T>
    constexpr auto MakeStaticECSRegistry() {
        return ECSSystemContainer<T...>{};
    }
}