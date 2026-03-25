# 06. Core Message Types & Data Dictionary

The Hermes Standard defines specific payloads for every stage of the handshake. This document provides a quick reference for the exact C++ structs and fields you will interact with in `HermesData.hpp`.

## ⚠️ Important Note on `Hermes::Optional<T>`
To maintain compatibility with older C++ compilers, this library does not use `std::optional`. It uses a custom `Hermes::Optional<T>`. To safely read optional fields like barcodes or dimensions, check them like a boolean and dereference them with `*`:

```cpp
if (boardData.m_optionalTopBarcode) {
    std::string barcode = *boardData.m_optionalTopBarcode;
}
1. Initialization Messages
ServiceDescriptionData
Exchanged immediately upon TCP connection to verify compatibility.

std::string m_machineId: The name/ID of the machine.

unsigned m_laneId: The specific lane (usually 0 or 1).

std::string m_version: Protocol version (Defaults to "1.5").

SupportedFeatures m_supportedFeatures: Flags for advanced features.

2. Board Handover Messages
BoardAvailableData (Sent by Upstream)
Announces a board is waiting to be sent. Contains physical properties.
Required Fields:

std::string m_boardId: Unique UUID for the board.

std::string m_boardIdCreatedBy: Machine ID that generated the UUID.

EBoardQuality m_failedBoard: eANY, eGOOD, or eBAD.

EFlippedBoard m_flippedBoard: eSIDE_UP_IS_UNKNOWN, eTOP_SIDE_IS_UP, eBOTTOM_SIDE_IS_UP.

Common Optional Fields:

Optional<std::string> m_optionalTopBarcode / m_optionalBottomBarcode

Optional<double> m_optionalLengthInMM / m_optionalWidthInMM / m_optionalThicknessInMM

Optional<std::string> m_optionalWorkOrderId / m_optionalBatchId

MachineReadyData (Sent by Downstream)
Announces the receiver is empty and ready.

EBoardQuality m_failedBoard: Specifies what quality the machine accepts (e.g., eGOOD).

Note: This struct also contains the same optional physical dimensions as BoardAvailableData. Downstream machines can populate these to demand specific board sizes.

3. Transport Control Messages
StartTransportData (Sent by Downstream)
Commands the Upstream machine to begin pushing the board.

std::string m_boardId: The UUID of the board being requested.

Optional<double> m_optionalConveyorSpeedInMMPerSecs: The speed the sender should match.

TransportFinishedData (Sent by Upstream)
Confirms the board has entirely left the sender's conveyor.

ETransferState m_transferState: Usually ETransferState::eCOMPLETE.

std::string m_boardId: The UUID of the transferred board.

StopTransportData (Sent by Downstream)
Confirms the board has securely arrived inside the receiver.

ETransferState m_transferState: Usually ETransferState::eCOMPLETE.

std::string m_boardId: The UUID of the received board.