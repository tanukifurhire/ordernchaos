/*************************************************************************
  \file       GGSystemDebug.cpp
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
#include <GGSystemDebug.hpp>
//#include <GGEngine.hpp>

namespace Systems {

    //template<IsSystemManager T, SystemPriority Priority>
    //void SystemPriorityRegister<T, Priority>::Update() {

    //    try {
    //        //throw std::runtime_error("testing exception");                // use to test exception, this debugger can be removed
    //        std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
    //        //T::Update();
    //        auto duration = std::chrono::steady_clock::now() - currentTime;
    //        std::cout << "Time taken: " <<
    //            std::chrono::duration<double>(duration).count() << "sec" << std::endl;
    //    }
    //    catch (const std::exception& e)
    //    {
    //        std::ofstream log("crash_log.txt", std::ios::app);              // writes new data instead of overwriting
    //        if (log.is_open()) {

    //            auto systemTime = std::chrono::system_clock::now();
    //            std::time_t _systemTime = std::chrono::system_clock::to_time_t(systemTime);

    //            log << "*****************************************" << std::endl;
    //            log << "Current Date & Time: "
    //                << std::put_time(std::localtime(&_systemTime), "%d/%m/%Y %H:%M:%S")
    //                << std::endl;

    //            log << "ERROR: [" << typeid(T).name() << "] Update failed: "
    //                << e.what() << std::endl;
    //        }
    //        throw std::runtime_error(e.what());
    //    }
    //    //std::cout << "This is a System Priority Register Update for Typename: " << typeid(T).name() << std::endl;
    //}
}