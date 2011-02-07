//  Copyright (c) 2007-2011 Hartmut Kaiser
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx.hpp>
#include <hpx/runtime/components/component_factory.hpp>
#include <hpx/util/portable_binary_iarchive.hpp>
#include <hpx/util/portable_binary_oarchive.hpp>

#include <hpx/components/distributing_factory/server/distributing_factory.hpp>

#include <boost/serialization/version.hpp>
#include <boost/serialization/export.hpp>

///////////////////////////////////////////////////////////////////////////////
// Add factory registration functionality
HPX_REGISTER_COMPONENT_MODULE();

///////////////////////////////////////////////////////////////////////////////
typedef hpx::components::server::distributing_factory distributing_factory_type;

HPX_REGISTER_MINIMAL_COMPONENT_FACTORY(
    hpx::components::simple_component<distributing_factory_type>, 
    distributing_factory);

///////////////////////////////////////////////////////////////////////////////
// Serialization support for the distributing_factory actions
HPX_REGISTER_ACTION_EX(
    distributing_factory_type::create_components_action,
    distributing_factory_create_components_action);
// HPX_REGISTER_ACTION_EX(
//     distributing_factory_type::free_components_action,
//     distributing_factory_free_components_action);
HPX_DEFINE_GET_COMPONENT_TYPE(distributing_factory_type);

typedef hpx::lcos::base_lco_with_value<
        distributing_factory_type::remote_result_type 
    > create_result_type;
typedef hpx::lcos::base_lco_with_value<
        distributing_factory_type::result_type, 
        distributing_factory_type::remote_result_type 
    > remote_create_result_type;

HPX_REGISTER_ACTION_EX(
    create_result_type::set_result_action,
    set_result_action_distributing_factory_result);
HPX_DEFINE_GET_COMPONENT_TYPE_STATIC(
    create_result_type, hpx::components::component_base_lco_with_value);
HPX_DEFINE_GET_COMPONENT_TYPE_STATIC(
    remote_create_result_type, hpx::components::component_base_lco_with_value);

