// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_future.h"

namespace RocProfVis
{
namespace DataModel
{
    class ProfileDatabase;
    class RocprofDatabase;
    class RocpdDatabase;

    class DatabaseVersion
    {
    public:

        void SetVersion(const char* version);
        bool IsVersionEqual(const char*);
        bool IsVersionGreaterOrEqual(const char*);

        uint32_t GetMajorVersion()
        {
            return m_db_version.size() > 0 ? m_db_version[0] : 0;
        }
        uint32_t GetMinorVersion()
        {
            return m_db_version.size() > 1 ? m_db_version[1] : 0;
        }
        uint32_t GetPatchVersion()
        {
            return m_db_version.size() > 2 ? m_db_version[2] : 0;
        }
    private:
        std::vector<uint32_t>  ConvertVersionStringToInt(const char* version);
        std::vector<uint32_t> m_db_version;
    };

	class MetadataVersionControl
	{
	public:
        enum roc_optiq_table_type : uint16_t
        {
            kRocOptiqTablePerFile,
            kRocOptiqTablePerGuid,
        };

        enum roc_optiq_table_trim_type : uint16_t
        {
            kRocOptiqTableCopyWhenTrimmed,
            kRocOptiqTableDisposeWhenTrimmed,
        };

        enum roc_optiq_table_version_t : uint32_t
        {
            kRocOptiqTableVersionMemoryActivity = 0x0002,
            kRocOptiqTableVersionMemoryAllocate = 0x0001,
            kRocOptiqTableVersionKernelDispatchLevel = 0x0001,
            kRocOptiqTableVersionRegionLevel = 0x0001,
            kRocOptiqTableVersionRegionSampleLevel = 0x0001,
            kRocOptiqTableVersionMemoryAllocLevel = 0x0001,
            kRocOptiqTableVersionMemoryCopyLevel = 0x0001,
            kRocOptiqTableVersionHistogram = 0x0001,
            kRocOptiqTableVersionTrackInfo = 0x0001,
        };

        struct roc_optiq_metadata_t
        {
            std::string name;
            roc_optiq_table_type type;
            roc_optiq_table_trim_type trim_type;
            uint16_t dependency;
            uint32_t version;
            uint64_t hash;
            std::map<uint32_t, bool> rebuild;
        };

        inline static const char* s_metadata_table_name = "roc_optiq_metadata";
        inline static const char* s_metadata_cache_table_name = "Metadata";

        inline static std::vector<std::string> s_pre_metadata_base_names = {
            "event_levels_",
            "histogram_",
            "track_info_",
            "roc_optiq"
        };

        MetadataVersionControl(ProfileDatabase* db): m_db(db), m_rebuild_all(false) {}
        rocprofvis_dm_result_t VerifyRocOptiqTablesVersions(Future* future);
        rocprofvis_dm_result_t DropAllRocOtiqTables(Future* future, uint32_t file_node_id);
        rocprofvis_dm_result_t CleanupDatabase(Future* future, bool rebuild);
        bool MustRebuild(uint32_t file_node_id, uint8_t id) { return m_rebuild_all || m_roc_optiq_table_properties[id].rebuild[file_node_id]; };
        const char* GetTableName(uint8_t id) { return m_roc_optiq_table_properties[id].name.c_str(); }
        bool DisposeTableWhenTrimming(std::string table_name);
        virtual const char* GetHistogramTableName() = 0;
        virtual const char* GetTrackInfoTableName() = 0;
        virtual bool MustRebuildHistogram(uint32_t file_node_id) = 0;
        virtual bool MustRebuildTrackInfo(uint32_t file_node_id) = 0;
    private:
        ProfileDatabase* m_db;
    protected:
        std::vector<roc_optiq_metadata_t> m_roc_optiq_table_properties;
        bool m_rebuild_all;
	};

    class RocprofMetadataVersionControl : public MetadataVersionControl
    {
    public:
        enum roc_optiq_tables : uint8_t
        {          
            kRocOptiqTableMemoryActivity,
            kRocOptiqTableMemoryAllocate,
            kRocOptiqTableTrackInfo,
            kRocOptiqTableKernelDispatchLevel,
            kRocOptiqTableRegionLevel,
            kRocOptiqTableRegionSampleLevel,
            kRocOptiqTableMemoryAllocLevel,
            kRocOptiqTableMemoryCopyLevel,
            kRocOptiqTableHistogram,

            kRocOptiqNumTables
        };

        enum roc_optiq_table_dependency_mask_t : uint16_t
        {
            kRocOptiqTableDependentOnNoDependency = 0,
            kRocOptiqTableDependentOnTrackInfo = 1 << kRocOptiqTableTrackInfo,
            kRocOptiqTableDependentOnMemoryActivity = 1 << kRocOptiqTableMemoryActivity,
            kRocOptiqTableDependentOnMemoryAllocate = 1 << kRocOptiqTableMemoryAllocate,
            kRocOptiqTableDependentOnMemoryVisibility = 
            kRocOptiqTableDependentOnMemoryActivity | 
            kRocOptiqTableDependentOnMemoryAllocate,
            kRocOptiqTableDependentOnKernelDispatchLevel = 1 << kRocOptiqTableKernelDispatchLevel,
            kRocOptiqTableDependentOnRegionLevel = 1 << kRocOptiqTableRegionLevel,
            kRocOptiqTableDependentOnRegionSampleLevel = 1 << kRocOptiqTableRegionSampleLevel,
            kRocOptiqTableDependentOnMemoryAllocLevel = 1 << kRocOptiqTableMemoryAllocLevel,
            kRocOptiqTableDependentOnMemoryCopyLevel = 1 << kRocOptiqTableMemoryCopyLevel,
            kRocOptiqTableDependentOnAllLevelTables = 
            kRocOptiqTableDependentOnKernelDispatchLevel | 
            kRocOptiqTableDependentOnRegionLevel | 
            kRocOptiqTableDependentOnRegionSampleLevel | 
            kRocOptiqTableDependentOnMemoryAllocLevel | 
            kRocOptiqTableDependentOnMemoryCopyLevel | 
            kRocOptiqTableDependentOnTrackInfo,
            kRocOptiqTableDependentOnHistogram = 1 << kRocOptiqTableHistogram,

        };
        RocprofMetadataVersionControl(RocprofDatabase* db);
        bool MustRebuildLevels(uint32_t file_node_id) { 
            return MustRebuild(file_node_id, kRocOptiqTableKernelDispatchLevel) ||
                MustRebuild(file_node_id, kRocOptiqTableRegionLevel) ||
                MustRebuild(file_node_id, kRocOptiqTableRegionSampleLevel) ||
                MustRebuild(file_node_id, kRocOptiqTableMemoryAllocLevel) ||
                MustRebuild(file_node_id, kRocOptiqTableMemoryCopyLevel);
        }
        const char* GetHistogramTableName() override { return GetTableName(kRocOptiqTableHistogram); };
        const char* GetTrackInfoTableName() override { return GetTableName(kRocOptiqTableTrackInfo); };
        bool MustRebuildHistogram(uint32_t file_node_id) override { return MustRebuild(file_node_id, kRocOptiqTableHistogram); }
        bool MustRebuildTrackInfo(uint32_t file_node_id) override { return MustRebuild(file_node_id, kRocOptiqTableTrackInfo); }
         
    };
    class RocpdMetadataVersionControl : public MetadataVersionControl
    {
    public:
        enum roc_optiq_tables : uint8_t
        {
            kRocOptiqTableTrackInfo,
            kRocOptiqTableKernelDispatchLevel,
            kRocOptiqTableRegionLevel,
            kRocOptiqTableHistogram,

            kRocOptiqNumTables
        };
        enum roc_optiq_table_dependency_mask_t : uint16_t
        {
            kRocOptiqTableDependentOnNoDependency = 0,
            kRocOptiqTableDependentOnTrackInfo = 1 << kRocOptiqTableTrackInfo,
            kRocOptiqTableDependentOnKernelDispatchLevel = 1 << kRocOptiqTableKernelDispatchLevel,
            kRocOptiqTableDependentOnRegionLevel = 1 << kRocOptiqTableRegionLevel,
            kRocOptiqTableDependentOnAllLevelTables = 
            kRocOptiqTableDependentOnKernelDispatchLevel | 
            kRocOptiqTableDependentOnRegionLevel | 
            kRocOptiqTableDependentOnTrackInfo,
            kRocOptiqTableDependentOnHistogram = 1 << kRocOptiqTableHistogram,

        };
        RocpdMetadataVersionControl(RocpdDatabase* db);

        bool MustRebuildLevels() { 
            return MustRebuild(0, kRocOptiqTableKernelDispatchLevel) ||
                MustRebuild(0, kRocOptiqTableRegionLevel);
        }
        const char* GetHistogramTableName() override { return GetTableName(kRocOptiqTableHistogram); };
        const char* GetTrackInfoTableName() override { return GetTableName(kRocOptiqTableTrackInfo); };
        bool MustRebuildHistogram(uint32_t file_node_id) override { return MustRebuild(file_node_id, kRocOptiqTableHistogram); }
        bool MustRebuildTrackInfo(uint32_t file_node_id) override { return MustRebuild(file_node_id, kRocOptiqTableTrackInfo); }
    };

}  // namespace DataModel
}  // namespace RocProfVis