// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_dm_topology.h"

namespace RocProfVis
{
namespace DataModel
{

TopologyNode* TopologyNode::GetRootNode() {
	if (m_prev_node == nullptr)
	{
		return this;
	}
	else
	{
		return m_prev_node->GetRootNode();
	}
}

rocprofvis_dm_result_t   TopologyNode::SetBasicProperty(const char* name, uint64_t db_instance, rocprofvis_db_topology_data_type_t type, const char* value) {
	auto props_map = GetPropertiesMap();
	auto it = props_map->find(name);
	if (it != props_map->end())
	{
		bool is_topology_id = it->second == kRPVControllerQueueId || it->second == kRPVControllerCounterId || it->second == kRPVControllerProcessorId;
		switch (type)
		{
			case kRPVTopologyDataTypeInt:
			{
				uint64_t ival = std::atoll(value);
				if (is_topology_id)
				{
					ival |= db_instance << 54;
				}
				m_properties[it->second] = ival;
				break;
			}
			case kRPVTopologyDataTypeDouble:
			{
				double dval = std::atof(value);
				m_properties[it->second] = dval;
				break;
			}
			case kRPVTopologyDataTypeString:
			{
				m_properties[it->second] = value;
				break;
			}
			case kRPVTopologyDataTypeNull: 
			{
				if (value)
				{
					if (strcmp(value, "0") == 0)
					{
						m_properties[it->second] = (uint64_t)0;
					}
					else
					{
						m_properties[it->second] = value;
					}
				}
				break;
			}
		}
		return kRocProfVisDmResultSuccess;
	}
	return kRocProfVisDmResultNotLoaded;
}


rocprofvis_dm_result_t   TopologyNodeRoot::AddProperty(rocprofvis_dm_track_identifiers_t* track_identifiers, rocprofvis_db_topology_data_type_t type, const char* table, const char* name, void* value) {
	rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
	DbInstance* db_instance = (DbInstance*)track_identifiers->db_instance;
	TopologyNode* node = FindRelevantPropertyNode(track_identifiers, table);
	if (node)
	{
		node->SetBasicProperty(name, db_instance->GuidIndex(), type, (const char* )value);
	}

	node = FindRelevantTopologyNode(track_identifiers, table);
	if (node)
	{
		node->ProcessProperty(name, type, value);
	}

	return kRocProfVisDmResultSuccess;
}


bool TopologyNode::DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers) {
	bool result = track_identifiers->tag[GetLevelId()] == GetLevelTag();
	if (result)
	{
		result = track_identifiers->id[GetLevelId()] == m_id;
    }
	return result;
}

TopologyNode*  TopologyNode::FindNodeMatchingIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	auto it = std::find_if(m_children.begin(), m_children.end(),
		[track_identifiers](std::unique_ptr<TopologyNode>& node) 
		{
			return node->DoesThisNodeMatchIdentifiers(track_identifiers);
		});
	if (it != m_children.end())
	{
		return it->get();
	}
	else
	{
		return nullptr;
	}
}

TopologyNode* TopologyNode::FindRelevantPropertyNode(rocprofvis_dm_track_identifiers_t* track_identifiers, std::string table_name)
{
	if (TopologyNode* node = FindNodeMatchingIdentifiers(track_identifiers))
	{
		if (node->IsRelevantPropertyTableName(table_name))
		{
			return node;
		}
		else
		{
			return node->FindRelevantPropertyNode(track_identifiers, table_name);
		}
	}
	return nullptr;
}

TopologyNode* TopologyNode::FindRelevantTopologyNode(rocprofvis_dm_track_identifiers_t* track_identifiers, std::string table_name)
{
	if (TopologyNode* node = FindNodeMatchingIdentifiers(track_identifiers))
	{
		if (node->IsRelevantTopologyTableName(table_name))
		{
			return node;
		}
		else
		{
			return node->FindRelevantTopologyNode(track_identifiers, table_name);
		}
	}
	return nullptr;
}

rocprofvis_dm_result_t TopologyNode::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value) {
	auto GetPropertyType = [&](int index, uint64_t* value)
		{
			switch (index)
			{
			case 0:
				*value = kRPVDataTypeInt;
				return kRocProfVisDmResultSuccess;
			case 1:
				*value = kRPVDataTypeDouble;
				return kRocProfVisDmResultSuccess;
			case 2:
				*value = kRPVDataTypeString;
				return kRocProfVisDmResultSuccess;
			}
			return kRocProfVisDmResultInvalidParameter;
		};

	ROCPROFVIS_ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
	switch (property)
	{
		case kRPVControllerTopologyNodeType:
			*value = m_type;
			return kRocProfVisDmResultSuccess;
		case kRPVControllerTopologyNodeNumChildren:
			*value = m_children.size();
			return kRocProfVisDmResultSuccess;
		case kRPVControllerTopologyNodeNumProperties:
			*value = m_properties.size();
			return kRocProfVisDmResultSuccess;
		case kRPVControllerTopologyNodeTrack:
			return GetTrackId(*value);
		case kRPVControllerTopologyNodePropertyTypeIndexed:
		{
			ROCPROFVIS_ASSERT_MSG_RETURN(index < m_properties.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
			auto it = m_properties.begin();
			std::advance(it, index);
			return GetPropertyType(it->second.index(), value);
			ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultInvalidProperty);
		}
		case kRPVControllerTopologyNodePropertyTypeKeyed:
		{
			auto it = m_properties.find(index);
			ROCPROFVIS_ASSERT_MSG_RETURN(it!=m_properties.end(), ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultInvalidProperty);
			return GetPropertyType(it->second.index(), value);
		}
		case kRPVControllerTopologyNodePropertyValueIndexed:
		{
			ROCPROFVIS_ASSERT_MSG_RETURN(index < m_properties.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
			auto it = m_properties.begin();
			std::advance(it, index);
			if (std::holds_alternative<uint64_t>(it->second)) {
				*value = std::get<uint64_t>(it->second);
				return kRocProfVisDmResultSuccess;
			}
			ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
		}
		case kRPVControllerTopologyNodePropertyValueKeyed:
		{
			auto it = m_properties.find(index);
			ROCPROFVIS_ASSERT_MSG_RETURN(it!=m_properties.end(), ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultNotLoaded);
			if (std::holds_alternative<uint64_t>(it->second)) {
				*value = std::get<uint64_t>(it->second);
				return kRocProfVisDmResultSuccess;
			}
			ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
		}
		case kRPVControllerTopologyNodePropertyKeyIndexed:
		{
			ROCPROFVIS_ASSERT_MSG_RETURN(index < m_properties.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
			auto it = m_properties.begin();
			std::advance(it, index);
			*value = it->first;
			return kRocProfVisDmResultSuccess;
		}
	}
	ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultInvalidProperty);
}

rocprofvis_dm_result_t TopologyNode::GetPropertyAsDouble(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, double * value) {
	ROCPROFVIS_ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
	switch (property)
	{
		case kRPVControllerTopologyNodePropertyValueIndexed:
		{
			ROCPROFVIS_ASSERT_MSG_RETURN(index < m_properties.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
			auto it = m_properties.begin();
			std::advance(it, index);
			if (std::holds_alternative<double>(it->second)) {
				*value = std::get<double>(it->second);
				return kRocProfVisDmResultSuccess;
			}
			ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
		}
		case kRPVControllerTopologyNodePropertyValueKeyed:
		{
			auto it = m_properties.find(index);
			ROCPROFVIS_ASSERT_MSG_RETURN(it!=m_properties.end(), ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultInvalidProperty);
			if (std::holds_alternative<double>(it->second)) {
				*value = std::get<double>(it->second);
				return kRocProfVisDmResultSuccess;
			}
			ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
		}

	}
	ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultInvalidProperty);
}

rocprofvis_dm_result_t TopologyNode::GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value) {
	ROCPROFVIS_ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
	switch (property)
	{
	    case kRPVControllerTopologyNodeName:
		{
			if (m_name.empty())
			{
				m_name = GetNodeName();
			}
			*value = (char*)m_name.c_str();
			return kRocProfVisDmResultSuccess;
		}
		case kRPVControllerTopologyNodePropertyValueIndexed:
		{
			ROCPROFVIS_ASSERT_MSG_RETURN(index < m_properties.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
			auto it = m_properties.begin();
			std::advance(it, index);
			if (std::holds_alternative<std::string>(it->second)) {
				*value = (char*)std::get<std::string>(it->second).c_str();
				return kRocProfVisDmResultSuccess;
			}
			ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
		}
		case kRPVControllerTopologyNodePropertyValueKeyed:
		{
			auto it = m_properties.find(index);
			ROCPROFVIS_ASSERT_MSG_RETURN(it!=m_properties.end(), ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultInvalidProperty);
			if (std::holds_alternative<std::string>(it->second)) {
				*value = (char*)std::get<std::string>(it->second).c_str();
				return kRocProfVisDmResultSuccess;
			}
			ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
		}

	}
	ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultInvalidProperty);
}

rocprofvis_dm_result_t TopologyNode::GetPropertyAsHandle(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value) {
	ROCPROFVIS_ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
	switch (property)
	{
		case kRPVControllerTopologyNodeChildHandleIndexed:
		{
			ROCPROFVIS_ASSERT_MSG_RETURN(index < m_children.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
			*value = m_children[index].get();
			return kRocProfVisDmResultSuccess;
		}
	}
	ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
}



rocprofvis_dm_result_t TopologyNodeRoot::AddNode(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	if (TopologyNode* node = FindNodeMatchingIdentifiers(track_identifiers))
	{
		return node->AddNode(track_identifiers);
	}
	if (Builder::SPACESAVER_SERVICE_NAME == track_identifiers->tag[TRACK_ID_NODE])
	{
		m_children.push_back(std::make_unique<TopologyNodeRPDSystemNode>(track_identifiers, this));
	}
	else
	{
		m_children.push_back(std::make_unique<TopologyNodeSystemNode>(track_identifiers, this));
	}
	return m_children.back()->AddNode(track_identifiers);
}


rocprofvis_dm_result_t TopologyNodeSystemNode::AddNode(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	if (TopologyNode* node = FindNodeMatchingIdentifiers(track_identifiers))
	{
		return node->AddNode(track_identifiers);
	}
	if (track_identifiers->category == kRocProfVisDmRegionTrack || 
		track_identifiers->category == kRocProfVisDmRegionMainTrack || 
		track_identifiers->category == kRocProfVisDmRegionSampleTrack || 
		track_identifiers->category == kRocProfVisDmStreamTrack)
	{
		m_children.push_back(std::make_unique<TopologyNodeProcess>(track_identifiers,this));
		return m_children.back()->AddNode(track_identifiers);
	}
	else 
	if (track_identifiers->category == kRocProfVisDmKernelDispatchTrack || 
	track_identifiers->category == kRocProfVisDmMemoryAllocationTrack || 
	track_identifiers->category == kRocProfVisDmMemoryCopyTrack ||
    track_identifiers->category == kRocProfVisDmPmcTrack)
	{
			m_children.push_back(std::make_unique<TopologyNodeProcessor>(track_identifiers,this));
			return m_children.back()->AddNode(track_identifiers);
	} else
	if (track_identifiers->category == kRocProfVisDmNotATrack)
	{
		m_children.push_back(std::make_unique<TopologyNodeProcessor>(track_identifiers,this));
		return kRocProfVisDmResultSuccess;
	}
	return kRocProfVisDmResultNotSupported;
}


std::string TopologyNodeSystemNode::GetNodeName() {
	std::string name = "Node ";
	auto it = m_properties.find(kRPVControllerNodeHostName);
	if (it != m_properties.end())
	{
		name += std::get<std::string>(it->second);
	}
	return name;
}

rocprofvis_dm_result_t TopologyNodeProcess::AddNode(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	TopologyNode* node = FindNodeMatchingIdentifiers(track_identifiers);
	if (node == nullptr)
	{
		if (track_identifiers->category == kRocProfVisDmRegionSampleTrack)
		{
			m_children.push_back(std::make_unique<TopologyNodeThreadSampled>(track_identifiers, this));
		}
		else
		if (track_identifiers->category == kRocProfVisDmRegionMainTrack)
		{
			m_children.push_back(std::make_unique<TopologyNodeThreadInstrumented>(track_identifiers, this));
		}
		else
		if (track_identifiers->category == kRocProfVisDmRegionTrack)
		{
			m_children.push_back(std::make_unique<TopologyNodeThread>(track_identifiers, this));
		}
		else
		if (track_identifiers->category == kRocProfVisDmStreamTrack)
		{
			m_children.push_back(std::make_unique<TopologyNodeStream>(track_identifiers, this));
		}
		else
		{
			return kRocProfVisDmResultInvalidParameter;
		}
	}
	return kRocProfVisDmResultSuccess;
}

std::string TopologyNodeProcess::GetNodeName() {
	std::string name;
	auto it = m_properties.find(kRPVControllerProcessCommand);
	if (it != m_properties.end())
	{
		name += std::get<std::string>(it->second);
	}
	it = m_properties.find(kRPVControllerProcessId);
	if (it != m_properties.end())
	{
		name += "(";
		name += std::to_string(std::get<std::uint64_t>(it->second));
		name += ")";
	}
	return name;
}

rocprofvis_dm_result_t TopologyNodeProcessor::AddNode(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	TopologyNode* node = FindNodeMatchingIdentifiers(track_identifiers);
	if (node == nullptr)
	{
		if (track_identifiers->category == kRocProfVisDmMemoryCopyTrack)
		{
			m_children.push_back(std::make_unique<TopologyNodeMemoryCopy>(track_identifiers,  this));
		} else
		if (track_identifiers->category == kRocProfVisDmMemoryAllocationTrack)
		{
			m_children.push_back(std::make_unique<TopologyNodeMemoryAllocation>(track_identifiers,  this));
		} else
		if (track_identifiers->category == kRocProfVisDmKernelDispatchTrack)
		{
			m_children.push_back(std::make_unique<TopologyNodeKernelDispatch>(track_identifiers,  this));
		}
		else if (track_identifiers->category == kRocProfVisDmPmcTrack)
		{
		    m_children.push_back(std::make_unique<TopologyNodeCounter>(track_identifiers, this));
		}
		else
		{
			return kRocProfVisDmResultInvalidParameter;
		}
	}
	return kRocProfVisDmResultSuccess;
}

std::string TopologyNodeProcessor::GetNodeName() {
	std::string name;
	auto it = m_properties.find(kRPVControllerProcessorType);
	if (it != m_properties.end())
	{
		name += std::get<std::string>(it->second);
	}
	it = m_properties.find(kRPVControllerProcessorTypeIndex);
	if (it != m_properties.end())
	{
		name += std::to_string(std::get<std::uint64_t>(it->second));
	}
	name += ": ";
	it = m_properties.find(kRPVControllerProcessorProductName);
	if (it != m_properties.end())
	{
		name += std::get<std::string>(it->second);
	}

	return name;
}

std::string TopologyNodeThread::GetNodeName() {
	std::string name;
	auto it = m_properties.find(kRPVControllerThreadName);
	if (it != m_properties.end())
	{
		name += std::get<std::string>(it->second);
	}
	it = m_properties.find(kRPVControllerThreadId);
	if (it != m_properties.end())
	{
		name += "(";
		name += std::to_string(std::get<std::uint64_t>(it->second));
		name += ")";
	}

	return name;
}

std::string TopologyNodeThreadInstrumented::GetNodeName() {
	return TopologyNodeThread::GetNodeName() + "(I)";
}

std::string TopologyNodeThreadSampled::GetNodeName() {
	return TopologyNodeThread::GetNodeName() + "(S)";
}

rocprofvis_dm_result_t TopologyNodeThreadInstrumented::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value) {
	if (kRPVControllerTopologyNodePropertyValueKeyed == property && kRPVControllerThreadType == index)
	{
		*value = kRPVControllerThreadTypeInstrumented;
		return kRocProfVisDmResultSuccess;
	}
	else
	{
		return TopologyNodeThread::GetPropertyAsUint64(property, index, value);
	}
}

rocprofvis_dm_result_t TopologyNodeThreadSampled::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value) {
	if (kRPVControllerTopologyNodePropertyValueKeyed == property && kRPVControllerThreadType == index)
	{
		*value = kRPVControllerThreadTypeSampled;
		return kRocProfVisDmResultSuccess;
	}
	else
	{
		return TopologyNodeThread::GetPropertyAsUint64(property, index, value);
	}
}

bool TopologyNodeThreadInstrumented::DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	bool result = TopologyNodeThread::DoesThisNodeMatchIdentifiers(track_identifiers);
	if (result)
	{
		result = track_identifiers->category == kRocProfVisDmRegionMainTrack;
	}
	return result;
}

bool TopologyNodeThreadSampled::DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	bool result = TopologyNode::DoesThisNodeMatchIdentifiers(track_identifiers);
	if (result)
	{
		result = track_identifiers->category == kRocProfVisDmRegionSampleTrack;
	}
	return result;
}

std::string TopologyNodeQueue::GetNodeName() {
	std::string name;
	auto it = m_properties.find(kRPVControllerQueueName);
	if (it != m_properties.end())
	{
		name += std::get<std::string>(it->second);
		name += "(";
		name += std::to_string(m_pid);
		name += ")";
	}
	return name;
}

std::string TopologyNodeMemoryAllocation::GetNodeName() {
	std::string name = "Memory allocation ";
	name += "(";
	name += std::to_string(m_pid);
	name += ")";
	return name;
}

std::string TopologyNodeMemoryCopy::GetNodeName() {
	std::string name = "Memory copy ";
	name += "(";
	name += std::to_string(m_pid);
	name += ")";
	return name;
}

rocprofvis_dm_result_t TopologyNodeMemoryAllocation::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value) {
	if (kRPVControllerTopologyNodePropertyValueKeyed == property && kRPVControllerQueueId == index)
	{
		*value = kRocProfVisDmMemoryAllocationTrack;
		return kRocProfVisDmResultSuccess;
	}
	else
	{
		return TopologyNodeQueue::GetPropertyAsUint64(property, index, value);
	}
}

rocprofvis_dm_result_t TopologyNodeMemoryCopy::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value) {
	if (kRPVControllerTopologyNodePropertyValueKeyed == property && kRPVControllerQueueId == index)
	{
		*value = kRocProfVisDmMemoryCopyTrack;
		return kRocProfVisDmResultSuccess;
	}
	else
	{
		return TopologyNodeQueue::GetPropertyAsUint64(property, index, value);
	}
}

bool TopologyNodeQueue::DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	bool result = TopologyNode::DoesThisNodeMatchIdentifiers(track_identifiers);
	if (result)
	{
		result = m_pid == track_identifiers->process_id;
	}
	return result;
}

bool TopologyNodeMemoryAllocation::DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	bool result = TopologyNodeQueue::DoesThisNodeMatchIdentifiers(track_identifiers);
	if (result)
	{
		result = track_identifiers->category == kRocProfVisDmMemoryAllocationTrack;
	}
	return result;
}

bool TopologyNodeMemoryCopy::DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	bool result = TopologyNodeQueue::DoesThisNodeMatchIdentifiers(track_identifiers);
	if (result)
	{
		result = track_identifiers->category == kRocProfVisDmMemoryCopyTrack;
	}
	return result;
}

bool TopologyNodeKernelDispatch::DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	bool result = TopologyNodeQueue::DoesThisNodeMatchIdentifiers(track_identifiers);
	if (result)
	{
		result = track_identifiers->category == kRocProfVisDmKernelDispatchTrack;
	}
	return result;
}

std::string TopologyNodeStream::GetNodeName() {
	std::string name;
	auto it = m_properties.find(kRPVControllerStreamName);
	if (it != m_properties.end())
	{
		name += std::get<std::string>(it->second);
	}

	return name;
}

std::string TopologyNodeCounter::GetNodeName() {
	std::string name;
	auto it = m_properties.find(kRPVControllerCounterName);
	if (it != m_properties.end())
	{
		name += std::get<std::string>(it->second);
			name += "(";
			name += std::to_string(m_pid);
			name += ")";
	}

	return name;
}

bool TopologyNodeCounter::DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	bool result = TopologyNode::DoesThisNodeMatchIdentifiers(track_identifiers);
	if (result)
	{
		result = m_pid == track_identifiers->process_id;
	}
	return result;
}

rocprofvis_dm_result_t  TopologyNodeStream::ProcessProperty(const char* name, rocprofvis_db_topology_data_type_t type, void* value) {
	static std::string track_id_ref_str = "TrackRef";
	if (track_id_ref_str == name && type == kRPVTopologyDataTypeRef)
	{
		rocprofvis_dm_track_identifiers_t* track_identifiers = (rocprofvis_dm_track_identifiers_t*)value;
		auto it = std::find_if(m_children.begin(), m_children.end(),
			[track_identifiers](std::unique_ptr<TopologyNode>& node) {return node.get()->GetId() == track_identifiers->id[TRACK_ID_AGENT]; });
		if (it == m_children.end())
		{
			m_children.push_back(std::make_unique<TopologyNodeDownStreamProcessor>( track_identifiers, this));
			return m_children.back()->AddNode(track_identifiers);
		}
		else
		{
			it->get()->AddNode(track_identifiers);
		}
		
	}
	return kRocProfVisDmResultInvalidParameter;
}

TopologyNode* TopologyReferenceNode::FindReferencedNode()
{
	TopologyNode* node = GetRootNode()->FindNodeMatchingIdentifiers(&downstream_track_identifiers);
	while (node!=nullptr && node->GetType() != m_type)
	{
		node = node->FindNodeMatchingIdentifiers(&downstream_track_identifiers);
	}
	return node;
}

rocprofvis_dm_result_t          TopologyReferenceNode::GetPropertyAsUint64(rocprofvis_dm_property_t property,
	rocprofvis_dm_property_index_t index,
	uint64_t* value) {
	TopologyNode* node = FindReferencedNode();
	if (node)
	{
		switch (property)
		{
			case kRPVControllerTopologyNodeNumChildren:
				*value = m_children.size();
				return kRocProfVisDmResultSuccess;
			default:
				return node->GetPropertyAsUint64(property, index, value);
		}		
	}
	return kRocProfVisDmResultNotLoaded;
}
rocprofvis_dm_result_t          TopologyReferenceNode::GetPropertyAsDouble(rocprofvis_dm_property_t property,
	rocprofvis_dm_property_index_t index,
	double* value) {
	TopologyNode* node = FindReferencedNode();
	if (node)
	{
		return node->GetPropertyAsDouble(property, index, value);
	}
	return kRocProfVisDmResultNotLoaded;
}
rocprofvis_dm_result_t          TopologyReferenceNode::GetPropertyAsCharPtr(rocprofvis_dm_property_t property,
	rocprofvis_dm_property_index_t index,
	char** value) {
	TopologyNode* node = FindReferencedNode();
	if (node)
	{
		return node->GetPropertyAsCharPtr(property, index, value);
	}
	return kRocProfVisDmResultNotLoaded;
}
rocprofvis_dm_result_t TopologyReferenceNode::GetPropertyAsHandle(rocprofvis_dm_property_t property,
	rocprofvis_dm_property_index_t index,
	rocprofvis_dm_handle_t* value) {
	TopologyNode* node = FindReferencedNode();
	if (node)
	{
		switch (property)
		{
		case kRPVControllerTopologyNodeChildHandleIndexed:
		{
			ROCPROFVIS_ASSERT_MSG_RETURN(index < m_children.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
			*value = m_children[index].get();
			return kRocProfVisDmResultSuccess;
		}
		default:
			return node->GetPropertyAsHandle(property, index, value);
		}	
		
	}
	return kRocProfVisDmResultNotLoaded;
}

std::string TopologyReferenceNode::GetNodeName() {
	TopologyNode* node = FindReferencedNode();
	if (node)
	{
		return node->GetNodeName();
	}
	return "Error generation node name";
}

rocprofvis_dm_result_t TopologyNodeDownStreamProcessor::AddNode(rocprofvis_dm_track_identifiers_t* track_identifiers)
{
	ROCPROFVIS_ASSERT_MSG_RETURN(track_identifiers, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
	auto it = std::find_if(m_children.begin(), m_children.end(),
		[track_identifiers, this](std::unique_ptr<TopologyNode>& node) {return node.get()->GetId() == track_identifiers->id[TRACK_ID_QUEUE]; });
	if (it == m_children.end())
	{
		m_children.push_back(std::make_unique<TopologyNodeDownStreamQueue>( track_identifiers,this));
	}
	return kRocProfVisDmResultSuccess;
}

}  // namespace DataModel
}  // namespace RocProfVis