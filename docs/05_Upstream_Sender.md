# 05. Upstream Sender (The Handshake)

An **Upstream** node acts as the sending machine (e.g., a printer sending a board to a pick-and-place). It operates as a TCP Client, actively attempting to connect to the IP address and port of the next machine in the line.

To successfully hand over a board, you must follow the exact reverse of the Downstream handshake.

---

## 1. The Standard Handshake Sequence

If you send messages out of sequence, the receiving machine is required by the Hermes Standard to drop the connection.

| Upstream (Sender - YOU) | Direction | Downstream (Receiver) |
| :--- | :---: | :--- |
| `ServiceDescription` | ➔ | *(Establish protocol version)* |
| | ⬅ | `MachineReady` *(I have space for a board)* |
| `BoardAvailable` | ➔ | *(Contains Barcode/Dimensions)* |
| | ⬅ | `StartTransport` *(Conveyors turning, send it!)* |
| `TransportFinished`| ➔ | *(Board has left my machine)* |
| | ⬅ | `StopTransport` *(Board is fully inside me)* |

---

## 2. Implementing the Sender Logic

Using the `HermesModern.hpp` wrapper, here is how you implement a compliant sender.

*(Note: In a real machine application, you will also integrate your physical sensors—like board edge detectors—into this logic flow).*

```cpp
#include "HermesModern.hpp"
#include <iostream>

Hermes::Modern::Upstream sender(1);
const unsigned CURRENT_SESSION = 1;

// 1. Wait for the Downstream machine to say it is empty
sender.RegisterMachineReadyCallback([&](const Hermes::MachineReadyData& data) {
    std::cout << "Downstream is ready to receive a board.\n";
    
    // 2. When your machine finishes processing a board, announce it
    Hermes::BoardAvailableData boardData;
    boardData.m_boardId = "PCB_12345";
    boardData.m_topBarcode = "SN-998877";
    boardData.m_lengthInMM = 150.0f;
    boardData.m_widthInMM = 100.0f;
    sender.Signal(CURRENT_SESSION, boardData);
});

// 3. Downstream has started its conveyors and says "Send it"
sender.RegisterStartTransportCallback([&](const Hermes::StartTransportData& data) {
    std::cout << "Downstream conveyors running at " << data.m_conveyorSpeed << " mm/s\n";
    
    // 4. Start YOUR conveyors to physically move the board out.
    // ... (Wait for hardware sensor to confirm board left the machine) ...
    
    // 5. Tell the Downstream machine the board is fully on their side
    Hermes::TransportFinishedData finishedData;
    finishedData.m_transferState = Hermes::ETransferState::eCOMPLETE;
    sender.Signal(CURRENT_SESSION, finishedData);
});

// 6. Downstream confirms the board has safely arrived inside
sender.RegisterStopTransportCallback([&](const Hermes::StopTransportData& data) {
    std::cout << "Handover Complete! Board safely delivered.\n";
    
    // You are now free to process the next board.
});
3. Handling Exceptions
RevokeMachineReady: If you receive this before sending BoardAvailable, it means the downstream machine suddenly became unavailable (e.g., an operator hit the Emergency Stop). You must wait until you receive a new MachineReady message before trying to send your board.

Connection Loss: If the connection drops during transport, halt your conveyors immediately.