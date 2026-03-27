# 07. Network Configuration & Topology

Hermes operates over standard TCP/IP Ethernet. Because it uses standard networking rather than proprietary cables, machines can be wired together in different ways depending on the factory's infrastructure.

This document explains the physical setups and how to configure your `UpstreamSettings` and `DownstreamSettings` structs to match them.

---

## 1. Physical Topologies

### Point-to-Point (Direct Connection)
This is the simplest setup and the direct equivalent of legacy SMEMA. Two adjacent machines are connected directly to each other using a standard Ethernet cable. 
* **Pros:** Extremely secure; no interference from other factory traffic.
* **Cons:** Requires the machine to have multiple network interface cards (NICs) if it also needs to talk to the factory network (MES).

### Switched Fabric (Smart Factory)
All machines in the line plug into a central factory Ethernet switch. 
* **Pros:** A single physical cable handles both horizontal (machine-to-machine) and vertical (machine-to-MES) data.
* **Cons:** Requires strict IP filtering to ensure Machine A doesn't accidentally send a board to Machine C.

---

## 2. The Hermes Port Standard

By definition (IPC-HERMES-9852), the TCP port used for a connection is derived from the Lane ID.
* The Base Port is always **50100**.
* Port = Base Port + Lane ID.
* **Therefore, a standard single-lane machine ALWAYS communicates on Port 50101.**

---

## 3. Configuring the Receiver (`DownstreamSettings`)

The Receiver acts as a TCP Server. It opens a port and waits. 

```cpp
Hermes::DownstreamSettings settings;
settings.m_machineId = "Oven_01";
settings.m_port = 50101; 

// OPTIONAL BUT RECOMMENDED IN SWITCHED NETWORKS:
// If you leave this blank, ANY machine on the factory floor can connect to your receiver.
// By setting this, you force the socket to ONLY accept connections from the specific
// IP address of the machine physically positioned before you in the line.
settings.m_optionalClientAddress = "192.168.1.50"; 

// Advanced keep-alive timings (defaults are usually fine)
settings.m_checkAlivePeriodInSeconds = 60.0;
settings.m_reconnectWaitTimeInSeconds = 10.0;
4. Configuring the Sender (UpstreamSettings)
The Sender acts as a TCP Client. It actively reaches out across the network to find the next machine.

C++
Hermes::UpstreamSettings settings;
settings.m_machineId = "Printer_01";
settings.m_port = 50101;

// REQUIRED:
// You must explicitly tell the sender the IP address of the Downstream
// receiver it is supposed to push boards into.
settings.m_hostAddress = "192.168.1.51"; 

// Advanced keep-alive timings
settings.m_checkAlivePeriodInSeconds = 60.0;
settings.m_reconnectWaitTimeInSeconds = 10.0;
5. Network Resilience
The library handles network drops automatically. If a cable is unplugged, the OnDisconnected callback will fire. The Upstream node will then automatically attempt to reconnect to the m_hostAddress every m_reconnectWaitTimeInSeconds (default: 10 seconds) until the connection is restored.