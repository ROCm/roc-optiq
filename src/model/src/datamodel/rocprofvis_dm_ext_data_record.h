// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_common_types.h"

namespace RocProfVis
{
namespace DataModel
{

// ExtDataRecord is a data storage class for extended data parameters
class ExtDataRecord 
{
    public:
        // ExtDataRecord class constructor
        // @param category - extended data category string
        // @param name - extended data name string
        // @param data - extended data string
        ExtDataRecord(rocprofvis_dm_charptr_t category, rocprofvis_dm_charptr_t name,
                      rocprofvis_dm_charptr_t data, rocprofvis_db_data_type_t type,
                      rocprofvis_event_data_category_enum_t category_enum,
                      rocprofvis_dm_node_id_t db_instance)
        :
            m_category(category), m_name(name), m_data(data), m_type(type), m_category_enum(category_enum), m_db_instance(db_instance) {};
        // Returns pointer to extended data category string 
        rocprofvis_dm_charptr_t        Category() {return m_category.c_str();}
        // Returns pointer to extended data name string 
        rocprofvis_dm_charptr_t        Name() {return m_name.c_str();}
        // Returns pointer to extended data string 
        rocprofvis_dm_charptr_t        Data() {return m_data.c_str();}
        // Returns pointer to extended data string
        uint64_t                       Type() { return m_type; }
        uint64_t                       CategoryEnum() { return m_category_enum; }
        rocprofvis_dm_node_id_t        DbInstance() { return m_db_instance; }
        // Checks if category and name tags are equal to the ones stored in record
        // @param category - pointer to category string to compare
        // @param name - pointer to name string to compare
        // @return True if equal
        bool                           Equal(rocprofvis_dm_charptr_t category, rocprofvis_dm_charptr_t name) {return (m_category.compare(category) == 0) && (m_name.compare(name) == 0);}

    private:
        // category string object
        rocprofvis_dm_string_t              m_category;
        // name string object
        rocprofvis_dm_string_t              m_name;
        // data string object
        rocprofvis_dm_string_t              m_data;
        // data type
        rocprofvis_db_data_type_t           m_type;
        // data category enumeration
        rocprofvis_event_data_category_enum_t m_category_enum;
        // data node id
        rocprofvis_dm_node_id_t             m_db_instance;
        
};

// ArgumentRecord is a data storage class for argument data parameters
class ArgumentRecord 
{
public:
    // ExtDataRecord class constructor
    // @param category - extended data category string
    // @param name - extended data name string
    // @param data - extended data string
    ArgumentRecord(rocprofvis_dm_charptr_t name, rocprofvis_dm_charptr_t value, rocprofvis_dm_charptr_t type, uint32_t position)
        : m_name(name), m_value(value), m_type(type), m_position(position)
    {
    };

    // Returns pointer to argument name string 
    rocprofvis_dm_charptr_t        Name() {return m_name.c_str();}
    // Returns pointer to argument value string 
    rocprofvis_dm_charptr_t        Value() {return m_value.c_str();}
    // Returns pointer to argument type string
    rocprofvis_dm_charptr_t        Type() { return m_type.c_str(); }
    // Returns argument position 
    uint32_t        Position() { return m_position; }

private:
    // argument name
    rocprofvis_dm_string_t              m_name;
    // argument value
    rocprofvis_dm_string_t              m_value;
    // argument type
    rocprofvis_dm_string_t              m_type;
    // argument position
    uint32_t                            m_position;

};

}  // namespace DataModel
}  // namespace RocProfVis