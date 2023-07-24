#include "ns3/download-server.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("DownloadServer");
NS_OBJECT_ENSURE_REGISTERED(DownloadServer);

TypeId
DownloadServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DownloadServer")
            .SetParent<Application>()
            .SetGroupName("Cybertwin")
            .AddConstructor<DownloadServer>()
            .AddAttribute("CybertwinID",
                          "Cybertwin ID of the server.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&DownloadServer::m_cybertwinID),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("MaxBytes",
                          "Maximum bytes to send.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&DownloadServer::m_maxBytes),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("MaxTime",
                          "Maximum time to send.",
                          TimeValue(Seconds(0.5)),
                          MakeTimeAccessor(&DownloadServer::m_maxSendTime),
                          MakeTimeChecker());
    return tid;
}

DownloadServer::DownloadServer()
{
    NS_LOG_DEBUG("[DownloadServer] create DownloadServer.");
}

DownloadServer::DownloadServer(CYBERTWINID_t cybertwinID, CYBERTWIN_INTERFACE_LIST_t interfaces)
{
    NS_LOG_DEBUG("[DownloadServer] create DownloadServer.");
    NS_LOG_DEBUG("[DownloadServer] Cybertwin ID: " << cybertwinID);
    for (auto it = interfaces.begin(); it != interfaces.end(); ++it)
    {
        NS_LOG_DEBUG("[DownloadServer] Interface: " << it->first << " " << it->second);
    }
    m_cybertwinID = cybertwinID;
    m_interfaces = interfaces;
}

DownloadServer::~DownloadServer()
{
    NS_LOG_DEBUG("[DownloadServer] destroy DownloadServer.");
}

void
DownloadServer::StartApplication()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("[DownloadServer] Starting DownloadServer.");

    // Start listening
    Init();
    // CNRS Insertion

    Ptr<CybertwinNode> node = DynamicCast<CybertwinNode>(GetNode());
    Ptr<NameResolutionService> cnrs = node->GetCNRSApp();
    NS_LOG_DEBUG("[DownloadServer] Inserting Cybertwin " << m_cybertwinID << " into CNRS.");
    cnrs->InsertCybertwinInterfaceName(1000, m_interfaces);

}

void
DownloadServer::StopApplication()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("Stopping DownloadServer.");
    for (auto it = m_sendBytes.begin(); it != m_sendBytes.end(); ++it)
    {
        if (it->first)
        {
            it->first->Close();
        }
    }
}

void
DownloadServer::Init()
{
    // Create socket
#if MDTP_ENABLED
    // init data server and listen for incoming connections
    m_dtServer = new CybertwinDataTransferServer();
    m_dtServer->Setup(GetNode(), m_cybertwinId, m_globalInterfaces);
    m_dtServer->Listen();
    m_dtServer->SetNewConnectCreatedCallback(
        MakeCallback(&Cybertwin::NewMpConnectionCreatedCallback, this));
#else
    // init data server and listen for incoming connections  
    m_dtServer = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::TcpSocketFactory"));
    //FIXME: attention, the port number is hard-coded here
    m_dtServer->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_interfaces[0].second));
    m_dtServer->SetAcceptCallback(MakeCallback(&DownloadServer::ConnRequestCallback, this),
                                  MakeCallback(&DownloadServer::ConnCreatedCallback, this));
    m_dtServer->SetCloseCallbacks(MakeCallback(&DownloadServer::NormalCloseCallback, this),
                                  MakeCallback(&DownloadServer::ErrorCloseCallback, this));
    m_dtServer->Listen();

    NS_LOG_DEBUG("[App][DownloadServer] Server is listening on " << m_interfaces[0].second);
#endif

    m_maxSendTime = Seconds(0.5);
}

bool
DownloadServer::ConnRequestCallback(Ptr<Socket> socket, const Address &from)
{
    NS_LOG_FUNCTION(this << socket << from);
    return true;
}

void
DownloadServer::ConnCreatedCallback(Ptr<Socket> socket, const Address &from)
{
    NS_LOG_FUNCTION(this << socket << from);
    // put into the map
    NS_LOG_DEBUG("[App][DownloadServer] New connection with " << InetSocketAddress::ConvertFrom(from).GetIpv4() <<":"<< InetSocketAddress::ConvertFrom(from).GetPort());

    // set recv callback
    socket->SetRecvCallback(MakeCallback(&DownloadServer::RecvCallback, this));

    // start bulk send
    m_sendBytes[socket] = 0;
    m_startTimes[socket] = Simulator::Now();
    Simulator::ScheduleNow(&DownloadServer::BulkSend, this, socket);
}

void
DownloadServer::BulkSend(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_INFO("[App][DownloadServer] BulkSend");
    if (!socket)
    {
        NS_LOG_ERROR("[App][DownloadServer] Connection closed.");
        return;
    }

    if ((m_startTimes[socket] + m_maxSendTime < Simulator::Now()))
    //((m_maxBytes != 0) && (m_sendBytes[socket] > m_maxBytes)))
    {
        //NS_LOG_DEBUG("[App][DownloadServer] Start Time " << m_startTimes[socket] << " Now " << Simulator::Now() << " Max Time " << m_maxSendTime);
        NS_LOG_DEBUG("[App][DownloadServer] Send "
                     << m_sendBytes[socket] << " bytes"
                     << " during " << Simulator::Now() - m_startTimes[socket] << " seconds.");
        socket->Close();
        return;
    }

    // send data
    Ptr<Packet> packet = Create<Packet>(SYSTEM_PACKET_SIZE);
    int32_t sendSize = socket->Send(packet);
    if (sendSize <= 0)
    {
        //NS_LOG_ERROR("[App][DownloadServer] Send error.");
    }
    else
    {
        NS_LOG_INFO("[App][DownloadServer] Send " << sendSize << " bytes.");
        m_sendBytes[socket] += sendSize;
    }

    Simulator::Schedule(MicroSeconds(10), &DownloadServer::BulkSend, this, socket);
}

void
DownloadServer::RecvCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_INFO("[App][DownloadServer] RecvCallback");

    if (!socket)
    {
        NS_LOG_ERROR("[App][DownloadServer] Connection closed.");
        return;
    }

#if 0
    Ptr<Packet> packet = socket->Recv();
    uint32_t rxBytes = packet->GetSize();

    Address peername;
    socket->GetPeerName(peername);
    NS_LOG_DEBUG("[App][DownloadServer] Received "<< rxBytes << " bytes from " << InetSocketAddress::ConvertFrom(peername).GetIpv4() <<":"<< InetSocketAddress::ConvertFrom(peername).GetPort());
#endif
}

void
DownloadServer::NormalCloseCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Address peername;
    socket->GetPeerName(peername);
    NS_LOG_DEBUG("[App][DownloadServer] Normal Connection with " << InetSocketAddress::ConvertFrom(peername).GetIpv4() <<":"<< InetSocketAddress::ConvertFrom(peername).GetPort() << " closed.");
}

void
DownloadServer::ErrorCloseCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Address peername;
    socket->GetPeerName(peername);
    NS_LOG_DEBUG("[App][DownloadServer] Error Connection with " << InetSocketAddress::ConvertFrom(peername).GetIpv4() <<":"<< InetSocketAddress::ConvertFrom(peername).GetPort() << " closed.");
}


} // namespace ns3