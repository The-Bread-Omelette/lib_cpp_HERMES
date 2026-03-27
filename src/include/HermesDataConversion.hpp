/***********************************************************************
Copyright 2018 ASM Assembly Systems GmbH & Co. KG

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
************************************************************************/
#pragma once

#include "Hermes.h"
#include "HermesData.hpp"
#include "HermesOptional.hpp"
#include "HermesStringView.hpp"

namespace Hermes
{
    // Verify constants match between C and C++ headers
    static_assert(cCONFIG_PORT     == cHERMES_CONFIG_PORT,      "config port mismatch");
    static_assert(cBASE_PORT       == cHERMES_BASE_PORT,        "base port mismatch");
    static_assert(cMAX_MESSAGE_SIZE == cHERMES_MAX_MESSAGE_SIZE, "max message size mismatch");

    // -------------------------------------------------------------------------
    // String conversions
    // -------------------------------------------------------------------------
    inline void CppToC(const std::string& data, HermesStringView& result)
    {
        result.m_pData = data.data();
        result.m_size  = data.size();
    }
    inline void CToCpp(HermesStringView data, std::string& result)
    {
        result.assign(data.m_pData, data.m_size);
    }
    inline HermesStringView ToC(StringView data)
    {
        return { data.data(), data.size() };
    }
    inline StringView ToCpp(HermesStringView data)
    {
        return { data.m_pData, data.m_size };
    }

    inline void CppToC(const Optional<std::string>& data, HermesStringView& result)
    {
        if (data.has_value()) { result.m_pData = data->data(); result.m_size = data->size(); }
        else                  { result.m_pData = nullptr;      result.m_size = 0; }
    }
    inline void CToCpp(HermesStringView data, Optional<std::string>& result)
    {
        result = data.m_pData ? Optional<std::string>(std::in_place, data.m_pData, data.m_size)
                              : std::nullopt;
    }

    // -------------------------------------------------------------------------
    // Scalar pass-throughs
    // FIX: removed separate uint32_t overload — on ARM/RPi, unsigned == uint32_t,
    // causing a redefinition error. One overload covers both.
    // -------------------------------------------------------------------------
    inline void CppToC(double   data, double&   result) { result = data; }
    inline void CToCpp(double   data, double&   result) { result = data; }
    inline void CppToC(uint16_t data, uint16_t& result) { result = data; }
    inline void CToCpp(uint16_t data, uint16_t& result) { result = data; }
    inline void CppToC(unsigned data, unsigned& result)  { result = data; }
    inline void CToCpp(unsigned data, unsigned& result)  { result = data; }

    // -------------------------------------------------------------------------
    // Optional pointer conversions
    // -------------------------------------------------------------------------
    template<class CT, class CppT>
    void CToCpp(const CT* pData, CppT& result)
    {
        if (pData) CToCpp(*pData, result);
    }

    template<class T>
    void CppToC(const Optional<T>& data, const T*& result)
    {
        result = data.has_value() ? &*data : nullptr;
    }

    template<class CT, class CppT>
    void CToCpp(const CT* pData, Optional<CppT>& result)
    {
        if (!pData) { result = std::nullopt; return; }
        CppT v{};
        CToCpp(*pData, v);
        result = std::move(v);
    }

    template<class CppT, class CT>
    void CppToC(const Optional<CppT>& data, CT& intermediate, const CT*& result)
    {
        if (!data.has_value()) { result = nullptr; return; }
        CppToC(*data, intermediate);
        result = &intermediate;
    }

    // -------------------------------------------------------------------------
    // Vector conversions
    // -------------------------------------------------------------------------
    template<class CT>
    struct VectorHolder
    {
        std::vector<CT>        m_values;
        std::vector<const CT*> m_pointers;
    };

    template<class CppT, class CT, class CVector>
    void CppToC(const std::vector<CppT>& data, VectorHolder<CT>& intermediate, CVector& result)
    {
        auto sz = data.size();
        intermediate.m_values.resize(sz, CT{});
        intermediate.m_pointers.resize(sz);
        for (uint32_t i = 0; i < sz; ++i)
        {
            CppToC(data[i], intermediate.m_values[i]);
            intermediate.m_pointers[i] = &intermediate.m_values[i];
        }
        result.m_pData = sz == 0 ? nullptr : intermediate.m_pointers.data();
        result.m_size  = sz;
    }

    template<class CVector, class CppT>
    void CToCpp(const CVector& data, std::vector<CppT>& result)
    {
        result.resize(data.m_size);
        for (uint32_t i = 0; i < data.m_size; ++i)
            CToCpp(*data.m_pData[i], result[i]);
    }

    // -------------------------------------------------------------------------
    // Enum conversions
    // -------------------------------------------------------------------------
    static_assert(size(EState())    == cHERMES_STATE_ENUM_SIZE,      "enum mismatch");
    inline EHermesState ToC(EState d)       { return static_cast<EHermesState>(d); }
    inline EState ToCpp(EHermesState d)     { return static_cast<EState>(d); }

    static_assert(size(ETraceType()) == cHERMES_TRACE_TYPE_ENUM_SIZE, "enum mismatch");
    inline EHermesTraceType ToC(ETraceType d)      { return static_cast<EHermesTraceType>(d); }
    inline ETraceType ToCpp(EHermesTraceType d)    { return static_cast<ETraceType>(d); }

    static_assert(size(ECheckAliveType()) == cHERMES_CHECK_ALIVE_TYPE_ENUM_SIZE, "enum mismatch");
    inline void CppToC(ECheckAliveType d, EHermesCheckAliveType& r)  { r = static_cast<EHermesCheckAliveType>(d); }
    inline void CToCpp(EHermesCheckAliveType d, ECheckAliveType& r)  { r = static_cast<ECheckAliveType>(d); }

    static_assert(size(EBoardQuality()) == cHERMES_BOARD_QUALITY_ENUM_SIZE, "enum mismatch");
    inline void CppToC(EBoardQuality d, EHermesBoardQuality& r)  { r = static_cast<EHermesBoardQuality>(d); }
    inline void CToCpp(EHermesBoardQuality d, EBoardQuality& r)  { r = static_cast<EBoardQuality>(d); }

    static_assert(size(EFlippedBoard()) == cHERMES_FLIPPED_BOARD_ENUM_SIZE, "enum mismatch");
    inline void CppToC(EFlippedBoard d, EHermesFlippedBoard& r)  { r = static_cast<EHermesFlippedBoard>(d); }
    inline void CToCpp(EHermesFlippedBoard d, EFlippedBoard& r)  { r = static_cast<EFlippedBoard>(d); }

    static_assert(size(ESubBoardState()) == cHERMES_SUB_BOARD_STATE_ENUM_SIZE, "enum mismatch");
    inline void CppToC(ESubBoardState d, EHermesSubBoardState& r)  { r = static_cast<EHermesSubBoardState>(d); }
    inline void CToCpp(EHermesSubBoardState d, ESubBoardState& r)  { r = static_cast<ESubBoardState>(d); }

    static_assert(size(ETransferState()) == cHERMES_TRANSFER_STATE_ENUM_SIZE, "enum mismatch");
    inline void CppToC(ETransferState d, EHermesTransferState& r)  { r = static_cast<EHermesTransferState>(d); }
    inline void CToCpp(EHermesTransferState d, ETransferState& r)  { r = static_cast<ETransferState>(d); }

    static_assert(size(ENotificationCode()) == cHERMES_NOTIFICATION_CODE_ENUM_SIZE, "enum mismatch");
    inline void CppToC(ENotificationCode d, EHermesNotificationCode& r)  { r = static_cast<EHermesNotificationCode>(d); }
    inline void CToCpp(EHermesNotificationCode d, ENotificationCode& r)  { r = static_cast<ENotificationCode>(d); }

    static_assert(size(ESeverity()) == cHERMES_SEVERITY_ENUM_SIZE, "enum mismatch");
    inline void CppToC(ESeverity d, EHermesSeverity& r)  { r = static_cast<EHermesSeverity>(d); }
    inline void CToCpp(EHermesSeverity d, ESeverity& r)  { r = static_cast<ESeverity>(d); }

    static_assert(size(ECheckState()) == cHERMES_CHECK_STATE_ENUM_SIZE, "enum mismatch");
    inline void CppToC(ECheckState d, EHermesCheckState& r)  { r = static_cast<EHermesCheckState>(d); }
    inline void CToCpp(EHermesCheckState d, ECheckState& r)  { r = static_cast<ECheckState>(d); }

    static_assert(size(EErrorCode()) == cHERMES_ERROR_CODE_ENUM_SIZE, "enum mismatch");
    inline void CppToC(EErrorCode d, EHermesErrorCode& r)  { r = static_cast<EHermesErrorCode>(d); }
    inline void CToCpp(EHermesErrorCode d, EErrorCode& r)  { r = static_cast<EErrorCode>(d); }

    static_assert(size(ECheckAliveResponseMode()) == cHERMES_CHECK_ALIVE_RESPONSE_MODE_ENUM_SIZE, "enum mismatch");
    inline void CppToC(ECheckAliveResponseMode d, EHermesCheckAliveResponseMode& r)  { r = static_cast<EHermesCheckAliveResponseMode>(d); }
    inline void CToCpp(EHermesCheckAliveResponseMode d, ECheckAliveResponseMode& r)  { r = static_cast<ECheckAliveResponseMode>(d); }

    static_assert(size(EBoardArrivedTransfer()) == cHERMES_BOARD_ARRIVED_TRANSFER_ENUM_SIZE, "enum mismatch");
    inline void CppToC(EBoardArrivedTransfer d, EHermesBoardArrivedTransfer& r)  { r = static_cast<EHermesBoardArrivedTransfer>(d); }
    inline void CToCpp(EHermesBoardArrivedTransfer d, EBoardArrivedTransfer& r)  { r = static_cast<EBoardArrivedTransfer>(d); }

    static_assert(size(EBoardDepartedTransfer()) == cHERMES_BOARD_DEPARTED_TRANSFER_ENUM_SIZE, "enum mismatch");
    inline void CppToC(EBoardDepartedTransfer d, EHermesBoardDepartedTransfer& r)  { r = static_cast<EHermesBoardDepartedTransfer>(d); }
    inline void CToCpp(EHermesBoardDepartedTransfer d, EBoardDepartedTransfer& r)  { r = static_cast<EBoardDepartedTransfer>(d); }

    static_assert(size(EReplyWorkOrderInfoStatus()) == cHERMES_REPLY_WORK_ORDER_INFO_STATUS_ENUM_SIZE, "enum mismatch");
    inline void CppToC(EReplyWorkOrderInfoStatus d, EHermesReplyWorkOrderInfoStatus& r)  { r = static_cast<EHermesReplyWorkOrderInfoStatus>(d); }
    inline void CToCpp(EHermesReplyWorkOrderInfoStatus d, EReplyWorkOrderInfoStatus& r)  { r = static_cast<EReplyWorkOrderInfoStatus>(d); }

    static_assert(size(EVerticalState()) == cHERMES_VERTICAL_STATE_ENUM_SIZE, "enum mismatch");
    inline EHermesVerticalState ToC(EVerticalState d)      { return static_cast<EHermesVerticalState>(d); }
    inline EVerticalState ToCpp(EHermesVerticalState d)    { return static_cast<EVerticalState>(d); }

    // -------------------------------------------------------------------------
    // Forward declarations
    // -------------------------------------------------------------------------
    template<class T> struct Converter2C;
    template<class T> struct ConverterBase
    {
        ConverterBase() = default;
        ConverterBase(const ConverterBase&)            = delete;
        ConverterBase(ConverterBase&&)                 = delete;
        ConverterBase& operator=(const ConverterBase&) = delete;
        ConverterBase& operator=(ConverterBase&&)      = delete;
        const T* CPointer() const { return &m_data; }
        T m_data{};
    };

    // -------------------------------------------------------------------------
    // UpstreamConfiguration / DownstreamConfiguration
    // -------------------------------------------------------------------------
    inline void CppToC(const UpstreamConfiguration& data, HermesUpstreamConfiguration& result)
    {
        CppToC(data.m_upstreamLaneId,              result.m_upstreamLaneId);
        CppToC(data.m_optionalUpstreamInterfaceId, result.m_optionalUpstreamInterfaceId);
        CppToC(data.m_hostAddress,                 result.m_hostAddress);
        CppToC(data.m_port,                        result.m_port);
    }
    inline void CToCpp(const HermesUpstreamConfiguration& data, UpstreamConfiguration& result)
    {
        CToCpp(data.m_upstreamLaneId,              result.m_upstreamLaneId);
        CToCpp(data.m_optionalUpstreamInterfaceId, result.m_optionalUpstreamInterfaceId);
        CToCpp(data.m_hostAddress,                 result.m_hostAddress);
        CToCpp(data.m_port,                        result.m_port);
    }

    inline void CppToC(const DownstreamConfiguration& data, HermesDownstreamConfiguration& result)
    {
        CppToC(data.m_downstreamLaneId,              result.m_downstreamLaneId);
        CppToC(data.m_optionalDownstreamInterfaceId, result.m_optionalDownstreamInterfaceId);
        CppToC(data.m_optionalClientAddress,         result.m_optionalClientAddress);
        CppToC(data.m_port,                          result.m_port);
    }
    inline void CToCpp(const HermesDownstreamConfiguration& data, DownstreamConfiguration& result)
    {
        CToCpp(data.m_downstreamLaneId,              result.m_downstreamLaneId);
        CToCpp(data.m_optionalDownstreamInterfaceId, result.m_optionalDownstreamInterfaceId);
        CToCpp(data.m_optionalClientAddress,         result.m_optionalClientAddress);
        CToCpp(data.m_port,                          result.m_port);
    }

    // -------------------------------------------------------------------------
    // SetConfigurationData
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<SetConfigurationData> : ConverterBase<HermesSetConfigurationData>
    {
        explicit Converter2C(const SetConfigurationData& data)
        {
            CppToC(data.m_machineId,                     m_data.m_machineId);
            CppToC(data.m_optionalSupervisorySystemPort, m_data.m_pOptionalSupervisorySystemPort);
            CppToC(data.m_upstreamConfigurations,   m_upstreamHolder,   m_data.m_upstreamConfigurations);
            CppToC(data.m_downstreamConfigurations, m_downstreamHolder, m_data.m_downstreamConfigurations);
        }
    private:
        VectorHolder<HermesUpstreamConfiguration>   m_upstreamHolder;
        VectorHolder<HermesDownstreamConfiguration> m_downstreamHolder;
    };
    inline SetConfigurationData ToCpp(const HermesSetConfigurationData& data)
    {
        SetConfigurationData result;
        CToCpp(data.m_machineId,                      result.m_machineId);
        CToCpp(data.m_pOptionalSupervisorySystemPort, result.m_optionalSupervisorySystemPort);
        CToCpp(data.m_upstreamConfigurations,         result.m_upstreamConfigurations);
        CToCpp(data.m_downstreamConfigurations,       result.m_downstreamConfigurations);
        return result;
    }

    // -------------------------------------------------------------------------
    // GetConfigurationData
    // -------------------------------------------------------------------------
    template<> struct Converter2C<GetConfigurationData> : ConverterBase<HermesGetConfigurationData>
    { explicit Converter2C(const GetConfigurationData&) {} };
    inline GetConfigurationData ToCpp(const HermesGetConfigurationData&) { return {}; }

    // -------------------------------------------------------------------------
    // CurrentConfigurationData
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<CurrentConfigurationData> : ConverterBase<HermesCurrentConfigurationData>
    {
        explicit Converter2C(const CurrentConfigurationData& data)
        {
            CppToC(data.m_optionalMachineId,             m_data.m_optionalMachineId);
            CppToC(data.m_optionalSupervisorySystemPort, m_data.m_pOptionalSupervisorySystemPort);
            CppToC(data.m_upstreamConfigurations,   m_upstreamHolder,   m_data.m_upstreamConfigurations);
            CppToC(data.m_downstreamConfigurations, m_downstreamHolder, m_data.m_downstreamConfigurations);
        }
    private:
        VectorHolder<HermesUpstreamConfiguration>   m_upstreamHolder;
        VectorHolder<HermesDownstreamConfiguration> m_downstreamHolder;
    };
    inline CurrentConfigurationData ToCpp(const HermesCurrentConfigurationData& data)
    {
        CurrentConfigurationData result;
        CToCpp(data.m_optionalMachineId,              result.m_optionalMachineId);
        CToCpp(data.m_pOptionalSupervisorySystemPort, result.m_optionalSupervisorySystemPort);
        CToCpp(data.m_upstreamConfigurations,         result.m_upstreamConfigurations);
        CToCpp(data.m_downstreamConfigurations,       result.m_downstreamConfigurations);
        return result;
    }

    // -------------------------------------------------------------------------
    // ConnectionInfo
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<ConnectionInfo> : ConverterBase<HermesConnectionInfo>
    {
        explicit Converter2C(const ConnectionInfo& data)
        {
            CppToC(data.m_address,  m_data.m_address);
            CppToC(data.m_port,     m_data.m_port);
            CppToC(data.m_hostName, m_data.m_hostName);
        }
    };
    inline ConnectionInfo ToCpp(const HermesConnectionInfo& data)
    {
        ConnectionInfo result;
        CToCpp(data.m_address,  result.m_address);
        CToCpp(data.m_port,     result.m_port);
        CToCpp(data.m_hostName, result.m_hostName);
        return result;
    }

    // -------------------------------------------------------------------------
    // Error
    // FIX: was Converter2C<e> (typo) — corrected to Converter2C<Error>
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<Error> : ConverterBase<HermesError>
    {
        explicit Converter2C(const Error& data)
        {
            CppToC(data.m_code, m_data.m_code);
            CppToC(data.m_text, m_data.m_text);
        }
    };
    inline Error ToCpp(const HermesError& data)
    {
        Error result;
        CToCpp(data.m_code, result.m_code);
        CToCpp(data.m_text, result.m_text);
        return result;
    }

    // -------------------------------------------------------------------------
    // CheckAliveData
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<CheckAliveData> : ConverterBase<HermesCheckAliveData>
    {
        explicit Converter2C(const CheckAliveData& data)
        {
            CppToC(data.m_optionalType, m_optType, m_data.m_pOptionalType);
            CppToC(data.m_optionalId,   m_data.m_optionalId);
        }
    private:
        EHermesCheckAliveType m_optType{};
    };
    inline CheckAliveData ToCpp(const HermesCheckAliveData& data)
    {
        CheckAliveData result;
        CToCpp(data.m_pOptionalType, result.m_optionalType);
        CToCpp(data.m_optionalId,    result.m_optionalId);
        return result;
    }

    // -------------------------------------------------------------------------
    // NotificationData
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<NotificationData> : ConverterBase<HermesNotificationData>
    {
        explicit Converter2C(const NotificationData& data)
        {
            CppToC(data.m_notificationCode, m_data.m_notificationCode);
            CppToC(data.m_severity,         m_data.m_severity);
            CppToC(data.m_description,      m_data.m_description);
        }
    };
    inline NotificationData ToCpp(const HermesNotificationData& data)
    {
        NotificationData result;
        CToCpp(data.m_notificationCode, result.m_notificationCode);
        CToCpp(data.m_severity,         result.m_severity);
        CToCpp(data.m_description,      result.m_description);
        return result;
    }

    // -------------------------------------------------------------------------
    // ServiceDescriptionData
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<ServiceDescriptionData> : ConverterBase<HermesServiceDescriptionData>
    {
        explicit Converter2C(const ServiceDescriptionData& data)
        {
            CppToC(data.m_machineId,           m_data.m_machineId);
            CppToC(data.m_laneId,              m_data.m_laneId);
            CppToC(data.m_optionalInterfaceId, m_data.m_optionalInterfaceId);
            CppToC(data.m_version,             m_data.m_version);
            m_data.m_pSupportedFeatures = nullptr;
        }
    };
    inline ServiceDescriptionData ToCpp(const HermesServiceDescriptionData& data)
    {
        ServiceDescriptionData result;
        CToCpp(data.m_machineId,           result.m_machineId);
        CToCpp(data.m_laneId,              result.m_laneId);
        CToCpp(data.m_optionalInterfaceId, result.m_optionalInterfaceId);
        CToCpp(data.m_version,             result.m_version);
        return result;
    }

    // -------------------------------------------------------------------------
    // SubBoard / SubBoards helper
    // SubBoards is typedef'd as std::vector<SubBoard> in HermesData.hpp
    // -------------------------------------------------------------------------
    inline void CppToC(const SubBoard& data, HermesSubBoard& result)
    {
        CppToC(data.m_pos,        result.m_pos);
        CppToC(data.m_optionalBc, result.m_optionalBc);
        CppToC(data.m_st,         result.m_st);
    }
    inline void CToCpp(const HermesSubBoard& data, SubBoard& result)
    {
        CToCpp(data.m_pos,        result.m_pos);
        CToCpp(data.m_optionalBc, result.m_optionalBc);
        CToCpp(data.m_st,         result.m_st);
    }

    struct SubBoardsHolder
    {
        VectorHolder<HermesSubBoard> m_holder;
        HermesSubBoards              m_data{};
    };
    inline void CppToC(const SubBoards& data, SubBoardsHolder& holder)
    {
        CppToC(data, holder.m_holder, holder.m_data);
    }
    inline void CToCpp(const HermesSubBoards& data, SubBoards& result)
    {
        CToCpp(data, result);
    }

    // -------------------------------------------------------------------------
    // BoardAvailableData
    // FIX: field names corrected to match HermesData.hpp:
    //   m_length -> m_optionalLengthInMM
    //   m_width  -> m_optionalWidthInMM
    //   etc.
    //   m_subBoards -> m_optionalSubBoards
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<BoardAvailableData> : ConverterBase<HermesBoardAvailableData>
    {
        explicit Converter2C(const BoardAvailableData& data)
        {
            CppToC(data.m_boardId,                    m_data.m_boardId);
            CppToC(data.m_boardIdCreatedBy,           m_data.m_boardIdCreatedBy);
            CppToC(data.m_failedBoard,                m_data.m_failedBoard);
            CppToC(data.m_optionalProductTypeId,      m_data.m_optionalProductTypeId);
            CppToC(data.m_flippedBoard,               m_data.m_flippedBoard);
            CppToC(data.m_optionalTopBarcode,         m_data.m_optionalTopBarcode);
            CppToC(data.m_optionalBottomBarcode,      m_data.m_optionalBottomBarcode);
            CppToC(data.m_optionalLengthInMM,         m_optLength,   m_data.m_pOptionalLengthInMM);
            CppToC(data.m_optionalWidthInMM,          m_optWidth,    m_data.m_pOptionalWidthInMM);
            CppToC(data.m_optionalThicknessInMM,      m_optThick,    m_data.m_pOptionalThicknessInMM);
            CppToC(data.m_optionalConveyorSpeedInMMPerSecs,    m_optSpeed,    m_data.m_pOptionalConveyorSpeedInMMPerSecs);
            CppToC(data.m_optionalTopClearanceHeightInMM,      m_optTopCl,    m_data.m_pOptionalTopClearanceHeightInMM);
            CppToC(data.m_optionalBottomClearanceHeightInMM,   m_optBotCl,    m_data.m_pOptionalBottomClearanceHeightInMM);
            CppToC(data.m_optionalWeightInGrams,      m_optWeight,   m_data.m_pOptionalWeightInGrams);
            CppToC(data.m_optionalWorkOrderId,        m_data.m_optionalWorkOrderId);
            CppToC(data.m_optionalBatchId,            m_data.m_optionalBatchId);
            CppToC(data.m_optionalRoute,              m_optRoute,    m_data.m_pOptionalRoute);
            CppToC(data.m_optionalAction,             m_optAction,   m_data.m_pOptionalAction);
            CppToC(data.m_optionalSubBoards,          m_subBoards);
            m_data.m_optionalSubBoards = m_subBoards.m_data;
        }
    private:
        double   m_optLength{}, m_optWidth{}, m_optThick{}, m_optSpeed{};
        double   m_optTopCl{}, m_optBotCl{}, m_optWeight{};
        uint16_t m_optRoute{}, m_optAction{};
        SubBoardsHolder m_subBoards;
    };
    inline BoardAvailableData ToCpp(const HermesBoardAvailableData& data)
    {
        BoardAvailableData result;
        CToCpp(data.m_boardId,               result.m_boardId);
        CToCpp(data.m_boardIdCreatedBy,      result.m_boardIdCreatedBy);
        CToCpp(data.m_failedBoard,           result.m_failedBoard);
        CToCpp(data.m_optionalProductTypeId, result.m_optionalProductTypeId);
        CToCpp(data.m_flippedBoard,          result.m_flippedBoard);
        CToCpp(data.m_optionalTopBarcode,    result.m_optionalTopBarcode);
        CToCpp(data.m_optionalBottomBarcode, result.m_optionalBottomBarcode);
        CToCpp(data.m_pOptionalLengthInMM,               result.m_optionalLengthInMM);
        CToCpp(data.m_pOptionalWidthInMM,                result.m_optionalWidthInMM);
        CToCpp(data.m_pOptionalThicknessInMM,            result.m_optionalThicknessInMM);
        CToCpp(data.m_pOptionalConveyorSpeedInMMPerSecs, result.m_optionalConveyorSpeedInMMPerSecs);
        CToCpp(data.m_pOptionalTopClearanceHeightInMM,   result.m_optionalTopClearanceHeightInMM);
        CToCpp(data.m_pOptionalBottomClearanceHeightInMM,result.m_optionalBottomClearanceHeightInMM);
        CToCpp(data.m_pOptionalWeightInGrams,            result.m_optionalWeightInGrams);
        CToCpp(data.m_optionalWorkOrderId,   result.m_optionalWorkOrderId);
        CToCpp(data.m_optionalBatchId,       result.m_optionalBatchId);
        CToCpp(data.m_pOptionalRoute,        result.m_optionalRoute);
        CToCpp(data.m_pOptionalAction,       result.m_optionalAction);
        CToCpp(data.m_optionalSubBoards,     result.m_optionalSubBoards);
        return result;
    }

    // -------------------------------------------------------------------------
    // RevokeBoardAvailableData
    // -------------------------------------------------------------------------
    template<> struct Converter2C<RevokeBoardAvailableData> : ConverterBase<HermesRevokeBoardAvailableData>
    { explicit Converter2C(const RevokeBoardAvailableData&) {} };
    inline RevokeBoardAvailableData ToCpp(const HermesRevokeBoardAvailableData&) { return {}; }

    // -------------------------------------------------------------------------
    // MachineReadyData
    // FIX: field names corrected (m_length -> m_optionalLengthInMM, etc.)
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<MachineReadyData> : ConverterBase<HermesMachineReadyData>
    {
        explicit Converter2C(const MachineReadyData& data)
        {
            CppToC(data.m_failedBoard,                    m_data.m_failedBoard);
            CppToC(data.m_optionalForecastId,             m_data.m_optionalForecastId);
            CppToC(data.m_optionalBoardId,                m_data.m_optionalBoardId);
            CppToC(data.m_optionalProductTypeId,          m_data.m_optionalProductTypeId);
            CppToC(data.m_optionalFlippedBoard,           m_optFlipped, m_data.m_pOptionalFlippedBoard);
            CppToC(data.m_optionalTopBarcode,             m_data.m_optionalTopBarcode);
            CppToC(data.m_optionalBottomBarcode,          m_data.m_optionalBottomBarcode);
            CppToC(data.m_optionalLengthInMM,             m_optLength, m_data.m_pOptionalLengthInMM);
            CppToC(data.m_optionalWidthInMM,              m_optWidth,  m_data.m_pOptionalWidthInMM);
            CppToC(data.m_optionalThicknessInMM,          m_optThick,  m_data.m_pOptionalThicknessInMM);
            CppToC(data.m_optionalConveyorSpeedInMMPerSecs, m_optSpeed, m_data.m_pOptionalConveyorSpeedInMMPerSecs);
            CppToC(data.m_optionalTopClearanceHeightInMM,   m_optTopCl, m_data.m_pOptionalTopClearanceHeightInMM);
            CppToC(data.m_optionalBottomClearanceHeightInMM,m_optBotCl, m_data.m_pOptionalBottomClearanceHeightInMM);
            CppToC(data.m_optionalWeightInGrams,          m_optWeight, m_data.m_pOptionalWeightInGrams);
            CppToC(data.m_optionalWorkOrderId,            m_data.m_optionalWorkOrderId);
            CppToC(data.m_optionalBatchId,                m_data.m_optionalBatchId);
        }
    private:
        EHermesFlippedBoard m_optFlipped{};
        double m_optLength{}, m_optWidth{}, m_optThick{}, m_optSpeed{};
        double m_optTopCl{}, m_optBotCl{}, m_optWeight{};
    };
    inline MachineReadyData ToCpp(const HermesMachineReadyData& data)
    {
        MachineReadyData result;
        CToCpp(data.m_failedBoard,            result.m_failedBoard);
        CToCpp(data.m_optionalForecastId,     result.m_optionalForecastId);
        CToCpp(data.m_optionalBoardId,        result.m_optionalBoardId);
        CToCpp(data.m_optionalProductTypeId,  result.m_optionalProductTypeId);
        CToCpp(data.m_pOptionalFlippedBoard,  result.m_optionalFlippedBoard);
        CToCpp(data.m_optionalTopBarcode,     result.m_optionalTopBarcode);
        CToCpp(data.m_optionalBottomBarcode,  result.m_optionalBottomBarcode);
        CToCpp(data.m_pOptionalLengthInMM,               result.m_optionalLengthInMM);
        CToCpp(data.m_pOptionalWidthInMM,                result.m_optionalWidthInMM);
        CToCpp(data.m_pOptionalThicknessInMM,            result.m_optionalThicknessInMM);
        CToCpp(data.m_pOptionalConveyorSpeedInMMPerSecs, result.m_optionalConveyorSpeedInMMPerSecs);
        CToCpp(data.m_pOptionalTopClearanceHeightInMM,   result.m_optionalTopClearanceHeightInMM);
        CToCpp(data.m_pOptionalBottomClearanceHeightInMM,result.m_optionalBottomClearanceHeightInMM);
        CToCpp(data.m_pOptionalWeightInGrams,            result.m_optionalWeightInGrams);
        CToCpp(data.m_optionalWorkOrderId,    result.m_optionalWorkOrderId);
        CToCpp(data.m_optionalBatchId,        result.m_optionalBatchId);
        return result;
    }

    // -------------------------------------------------------------------------
    // RevokeMachineReadyData
    // -------------------------------------------------------------------------
    template<> struct Converter2C<RevokeMachineReadyData> : ConverterBase<HermesRevokeMachineReadyData>
    { explicit Converter2C(const RevokeMachineReadyData&) {} };
    inline RevokeMachineReadyData ToCpp(const HermesRevokeMachineReadyData&) { return {}; }

    // -------------------------------------------------------------------------
    // StartTransportData
    // FIX: field name corrected: m_conveyorSpeed -> m_optionalConveyorSpeedInMMPerSecs
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<StartTransportData> : ConverterBase<HermesStartTransportData>
    {
        explicit Converter2C(const StartTransportData& data)
        {
            CppToC(data.m_boardId,                       m_data.m_boardId);
            CppToC(data.m_optionalConveyorSpeedInMMPerSecs, m_optSpeed, m_data.m_pOptionalConveyorSpeedInMMPerSecs);
        }
    private:
        double m_optSpeed{};
    };
    inline StartTransportData ToCpp(const HermesStartTransportData& data)
    {
        StartTransportData result;
        CToCpp(data.m_boardId,                           result.m_boardId);
        CToCpp(data.m_pOptionalConveyorSpeedInMMPerSecs, result.m_optionalConveyorSpeedInMMPerSecs);
        return result;
    }

    // -------------------------------------------------------------------------
    // StopTransportData
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<StopTransportData> : ConverterBase<HermesStopTransportData>
    {
        explicit Converter2C(const StopTransportData& data)
        {
            CppToC(data.m_transferState, m_data.m_transferState);
            CppToC(data.m_boardId,       m_data.m_boardId);
        }
    };
    inline StopTransportData ToCpp(const HermesStopTransportData& data)
    {
        StopTransportData result;
        CToCpp(data.m_transferState, result.m_transferState);
        CToCpp(data.m_boardId,       result.m_boardId);
        return result;
    }

    // -------------------------------------------------------------------------
    // TransportFinishedData
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<TransportFinishedData> : ConverterBase<HermesTransportFinishedData>
    {
        explicit Converter2C(const TransportFinishedData& data)
        {
            CppToC(data.m_transferState, m_data.m_transferState);
            CppToC(data.m_boardId,       m_data.m_boardId);
        }
    };
    inline TransportFinishedData ToCpp(const HermesTransportFinishedData& data)
    {
        TransportFinishedData result;
        CToCpp(data.m_transferState, result.m_transferState);
        CToCpp(data.m_boardId,       result.m_boardId);
        return result;
    }

    // -------------------------------------------------------------------------
    // QueryBoardInfoData
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<QueryBoardInfoData> : ConverterBase<HermesQueryBoardInfoData>
    {
        explicit Converter2C(const QueryBoardInfoData& data)
        {
            CppToC(data.m_optionalTopBarcode,    m_data.m_optionalTopBarcode);
            CppToC(data.m_optionalBottomBarcode, m_data.m_optionalBottomBarcode);
        }
    };
    inline QueryBoardInfoData ToCpp(const HermesQueryBoardInfoData& data)
    {
        QueryBoardInfoData result;
        CToCpp(data.m_optionalTopBarcode,    result.m_optionalTopBarcode);
        CToCpp(data.m_optionalBottomBarcode, result.m_optionalBottomBarcode);
        return result;
    }

    // -------------------------------------------------------------------------
    // CommandData
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<CommandData> : ConverterBase<HermesCommandData>
    {
        explicit Converter2C(const CommandData& data) { CppToC(data.m_command, m_data.m_command); }
    };
    inline CommandData ToCpp(const HermesCommandData& data)
    {
        CommandData result;
        CToCpp(data.m_command, result.m_command);
        return result;
    }

    // -------------------------------------------------------------------------
    // UpstreamSettings
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<UpstreamSettings> : ConverterBase<HermesUpstreamSettings>
    {
        explicit Converter2C(const UpstreamSettings& data)
        {
            CppToC(data.m_machineId,                   m_data.m_machineId);
            CppToC(data.m_hostAddress,                 m_data.m_hostAddress);
            CppToC(data.m_port,                        m_data.m_port);
            CppToC(data.m_checkAlivePeriodInSeconds,   m_data.m_checkAlivePeriodInSeconds);
            CppToC(data.m_reconnectWaitTimeInSeconds,  m_data.m_reconnectWaitTimeInSeconds);
            CppToC(data.m_checkAliveResponseMode,      m_data.m_checkAliveResponseMode);
            CppToC(data.m_checkState,                  m_data.m_checkState);
        }
    };
    inline UpstreamSettings ToCpp(const HermesUpstreamSettings& data)
    {
        UpstreamSettings result;
        CToCpp(data.m_machineId,                  result.m_machineId);
        CToCpp(data.m_hostAddress,                result.m_hostAddress);
        CToCpp(data.m_port,                       result.m_port);
        CToCpp(data.m_checkAlivePeriodInSeconds,  result.m_checkAlivePeriodInSeconds);
        CToCpp(data.m_reconnectWaitTimeInSeconds, result.m_reconnectWaitTimeInSeconds);
        CToCpp(data.m_checkAliveResponseMode,     result.m_checkAliveResponseMode);
        CToCpp(data.m_checkState,                 result.m_checkState);
        return result;
    }

    // -------------------------------------------------------------------------
    // DownstreamSettings
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<DownstreamSettings> : ConverterBase<HermesDownstreamSettings>
    {
        explicit Converter2C(const DownstreamSettings& data)
        {
            CppToC(data.m_machineId,                  m_data.m_machineId);
            CppToC(data.m_optionalClientAddress,      m_data.m_optionalClientAddress);
            CppToC(data.m_port,                       m_data.m_port);
            CppToC(data.m_checkAlivePeriodInSeconds,  m_data.m_checkAlivePeriodInSeconds);
            CppToC(data.m_reconnectWaitTimeInSeconds, m_data.m_reconnectWaitTimeInSeconds);
            CppToC(data.m_checkAliveResponseMode,     m_data.m_checkAliveResponseMode);
            CppToC(data.m_checkState,                 m_data.m_checkState);
        }
    };
    inline DownstreamSettings ToCpp(const HermesDownstreamSettings& data)
    {
        DownstreamSettings result;
        CToCpp(data.m_machineId,                  result.m_machineId);
        CToCpp(data.m_optionalClientAddress,      result.m_optionalClientAddress);
        CToCpp(data.m_port,                       result.m_port);
        CToCpp(data.m_checkAlivePeriodInSeconds,  result.m_checkAlivePeriodInSeconds);
        CToCpp(data.m_reconnectWaitTimeInSeconds, result.m_reconnectWaitTimeInSeconds);
        CToCpp(data.m_checkAliveResponseMode,     result.m_checkAliveResponseMode);
        CToCpp(data.m_checkState,                 result.m_checkState);
        return result;
    }

    // -------------------------------------------------------------------------
    // ConfigurationServiceSettings
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<ConfigurationServiceSettings> : ConverterBase<HermesConfigurationServiceSettings>
    {
        explicit Converter2C(const ConfigurationServiceSettings& data)
        {
            CppToC(data.m_port,                       m_data.m_port);
            CppToC(data.m_reconnectWaitTimeInSeconds, m_data.m_reconnectWaitTimeInSeconds);
        }
    };
    inline ConfigurationServiceSettings ToCpp(const HermesConfigurationServiceSettings& data)
    {
        ConfigurationServiceSettings result;
        CToCpp(data.m_port,                       result.m_port);
        CToCpp(data.m_reconnectWaitTimeInSeconds, result.m_reconnectWaitTimeInSeconds);
        return result;
    }

    // -------------------------------------------------------------------------
    // VerticalServiceSettings
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<VerticalServiceSettings> : ConverterBase<HermesVerticalServiceSettings>
    {
        explicit Converter2C(const VerticalServiceSettings& data)
        {
            CppToC(data.m_systemId,                   m_data.m_systemId);
            CppToC(data.m_port,                       m_data.m_port);
            CppToC(data.m_reconnectWaitTimeInSeconds, m_data.m_reconnectWaitTimeInSeconds);
            CppToC(data.m_checkAlivePeriodInSeconds,  m_data.m_checkAlivePeriodInSeconds);
            CppToC(data.m_checkAliveResponseMode,     m_data.m_checkAliveResponseMode);
        }
    };
    inline VerticalServiceSettings ToCpp(const HermesVerticalServiceSettings& data)
    {
        VerticalServiceSettings result;
        CToCpp(data.m_systemId,                   result.m_systemId);
        CToCpp(data.m_port,                       result.m_port);
        CToCpp(data.m_reconnectWaitTimeInSeconds, result.m_reconnectWaitTimeInSeconds);
        CToCpp(data.m_checkAlivePeriodInSeconds,  result.m_checkAlivePeriodInSeconds);
        CToCpp(data.m_checkAliveResponseMode,     result.m_checkAliveResponseMode);
        return result;
    }

    // -------------------------------------------------------------------------
    // VerticalClientSettings
    // -------------------------------------------------------------------------
    template<>
    struct Converter2C<VerticalClientSettings> : ConverterBase<HermesVerticalClientSettings>
    {
        explicit Converter2C(const VerticalClientSettings& data)
        {
            CppToC(data.m_systemId,                   m_data.m_systemId);
            CppToC(data.m_hostAddress,                m_data.m_hostAddress);
            CppToC(data.m_port,                       m_data.m_port);
            CppToC(data.m_reconnectWaitTimeInSeconds, m_data.m_reconnectWaitTimeInSeconds);
            CppToC(data.m_checkAlivePeriodInSeconds,  m_data.m_checkAlivePeriodInSeconds);
            CppToC(data.m_checkAliveResponseMode,     m_data.m_checkAliveResponseMode);
        }
    };
    inline VerticalClientSettings ToCpp(const HermesVerticalClientSettings& data)
    {
        VerticalClientSettings result;
        CToCpp(data.m_systemId,                   result.m_systemId);
        CToCpp(data.m_hostAddress,                result.m_hostAddress);
        CToCpp(data.m_port,                       result.m_port);
        CToCpp(data.m_reconnectWaitTimeInSeconds, result.m_reconnectWaitTimeInSeconds);
        CToCpp(data.m_checkAlivePeriodInSeconds,  result.m_checkAlivePeriodInSeconds);
        CToCpp(data.m_checkAliveResponseMode,     result.m_checkAliveResponseMode);
        return result;
    }

} // namespace Hermes