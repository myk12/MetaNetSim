#ifndef CYBERTWIN_MULTIPATH_CONTROLLER_H
#define CYBERTWIN_MULTIPATH_CONTROLLER_H
#include "cybertwin-common.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/log.h"
#include "ns3/random-variable-stream.h"
#include "cybertwin-tag.h"
#include "cybertwin-header.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>

using namespace ns3;

struct DataItem
{
    MpDataSeqNum seqStart;
    Ptr<Packet> packet;
    uint32_t length;
};

class SinglePath;
class MultipathConnection;
class CybertwinDataTransferServer;


//*****************************************************************************
//*                    Cybertwin Data Transfer Server                         *
//*****************************************************************************
// Multipath Control Block
// This class manage all the multipath connections with remote Cybertwins
class CybertwinDataTransferServer : public Object
{
public:
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;

    CybertwinDataTransferServer();
    void Setup(Ptr<Node> node, CYBERTWINID_t cyberid, CYBERTWIN_INTERFACE_LIST_t ifs);
    void Listen();

    void InsertCNRSCache(CYBERTWINID_t , CYBERTWIN_INTERFACE_LIST_t );

    void NewConnectionBuilt(SinglePath *path);
    bool ValidConnectionID(MP_CONN_ID_t connid);
    void NewPathJoinConnection(SinglePath* path);

    void SetNewConnectCreatedCallback(Callback<void, MultipathConnection*> newConnCb);
    void DtServerBulkSend(MultipathConnection* conn);

    // path management
private:
    // callbacks
    bool PathRequestCallback(Ptr<Socket> scok, const Address &addr);
    void PathCreatedCallback(Ptr<Socket> sock, const Address &addr);

    MP_CONN_KEY_t GenerateKey();

    // callbacks
    Callback<void, MultipathConnection*> m_notifyNewConnection;

    // identify
    Ptr<Node> m_node;
    CYBERTWINID_t m_localCybertwinID;
    CYBERTWIN_INTERFACE_LIST_t m_cybertwinIfs;

    // cybertwinID and Interfaces map
    std::unordered_map<CYBERTWINID_t, CYBERTWIN_INTERFACE_LIST_t> CNRSCache;
 
    Ptr<UniformRandomVariable> rand;
    std::unordered_set<MP_CONN_KEY_t> localKeys;

    // connections
    //std::unordered_map<SP_CONNID_t, MultipathConnection*> mpConnections
    std::unordered_set<MultipathConnection*> m_connections;
    std::unordered_map<MP_CONN_ID_t, MultipathConnection*> m_connectionIDs;
    
    // test number
    //uint64_t m_testNum;
    uint64_t m_rxBytes; // number of Sent KB
};

//*****************************************************************************
//*                     Multipath Connection                                  *
//*****************************************************************************
class MpRxBuffer
{
public:
    MpRxBuffer();
    int32_t AddItem(DataItem *item);
    MpDataSeqNum exceptSeqNum;
    std::queue<Ptr<Packet>> rxQueue;
    uint64_t size;
};

class MultipathConnection
{
public:
    MultipathConnection();
    MultipathConnection(SinglePath* path);

    enum MP_CONN_STATE
    {
        MP_CONN_INIT,
        MP_CONN_BIND,
        MP_CONN_SEND,
        MP_CONN_CONNECT,
        MP_CONN_CLOSING,
        MP_CONN_CLOSED,
    };

    Ptr<Packet> Recv();
    int32_t Send(Ptr<Packet> packet);
    //int32_t Listen();
    void Setup(Ptr<Node> node, CYBERTWINID_t localCybertwinID);
    void Connect(CYBERTWINID_t cyberid);
    int32_t Close();    //close the connection

    //data transfer
    void SendData();
    uint32_t ChoosePathRoundRobin();
    void PathRecvedData(SinglePath* path);

    //member accessor
    void InitIdentity(Ptr<Node> node,
                      CYBERTWINID_t localid,
                      CYBERTWINID_t remoteid,
                      MP_CONN_ID_t connid);

    void SetNode(Ptr<Node> node);
    void SetLocalKey(MP_CONN_KEY_t key);
    void SetRemoteKey(MP_CONN_KEY_t key);
    void SetLocalCybertwinID(CYBERTWINID_t id);
    void SetPeerCybertwinID(CYBERTWINID_t id);
    void SetConnID(MP_CONN_ID_t id);
    MP_CONN_ID_t GetConnID();
    void SetConnState(MP_CONN_STATE state);


    //callback
    void SetConnectCallback(Callback<void, MultipathConnection*> succeedCb,
                            Callback<void, MultipathConnection*> failCb);
    void SetRecvCallback(Callback<void, MultipathConnection*> recvCb);
    void SetCloseCallback(Callback<void, MultipathConnection*> closeCb);

    //path manangement
    void AddRawPath(SinglePath* path, bool ready);
    void AddInitConnectPath(SinglePath* path);
    void AddOtherConnectPath(SinglePath* path);
    void BuildConnection();
    void ConnectOtherPath();
    void PathJoinResult(SinglePath* path, bool success);

    void InsertCNRSItem(CYBERTWINID_t id, CYBERTWIN_INTERFACE_LIST_t ifs);
    //
    //MultipathConnection* GetConnFromLocalKey(MP_CONN_KEY_t key);



private:
    MP_CONN_ID_t m_connID;
    MP_CONN_STATE m_connState;
    Ptr<Node> m_node;
    MP_CONN_KEY_t m_localKey;
    MP_CONN_KEY_t m_remoteKey;
    CYBERTWINID_t m_localCyberID;
    CYBERTWINID_t m_peerCyberID;

    //data transfer
    std::vector<SinglePath*> m_paths;
    int32_t m_lastPathIndex;
    
    MpDataSeqNum m_sendSeqNum;
    std::queue<Ptr<Packet>> m_txBuffer;

    // test data transfer
    MpDataSeqNum m_recvSeqNum;
    std::queue<Ptr<Packet>> m_rxBuffer;


    //path queue
    std::queue<SinglePath*> m_readyPath;
    std::unordered_set<SinglePath*> m_errorPath;

    int32_t m_pathNum;
    std::queue<SinglePath*> m_rawReadyPath;
    std::queue<SinglePath*> m_rawFailPath;

    Ptr<UniformRandomVariable> rand;
    std::unordered_map<CYBERTWINID_t, CYBERTWIN_INTERFACE_LIST_t> CNRSCache;

    //callback
    Callback<void, MultipathConnection*> m_connectSucceedCallback;
    Callback<void, MultipathConnection*> m_connectFailedCallback;
    Callback<void, MultipathConnection*> m_recvCallback;
    Callback<void, MultipathConnection*> m_closeCallback;
};

//*****************************************************************************
//*                              Single Path                                  *
//*****************************************************************************
class SinglePath
{
public:
    friend class MultipathConnection;

    enum PathStatus
    {
        SINGLE_PATH_INIT,
        SINGLE_PATH_READY,
        SINGLE_PATH_LISTEN,
        SINGLE_PATH_BUILD_SENT,
        SINGLE_PATH_BUILD_RCVD,
        SINGLE_PATH_JOIN_SENT,
        SINGLE_PATH_JOIN_RCVD,
        SINGLE_PATH_CONNECTED,
        SINGLE_PATH_CLOSED,
        SINGLE_PATH_ERROR
    };
    SinglePath();

    //data transfer
    int32_t Send(Ptr<Packet> packet);
    Ptr<Packet> Recv();

    //path management
    int32_t PathConnect();
    int32_t PathListen();
    int32_t PathClose();
    void PathClean();

    //socket processer
    void PathRecvHandler(Ptr<Socket> socket);
    void PathCloseSucceeded(Ptr<Socket> socket);
    void PathCloseFailed(Ptr<Socket> socket);

    void PathConnectSucceeded(Ptr<Socket> sock);
    void PathConnectFailed(Ptr<Socket> sock);

    // the first path to build connection
    void InitConnection();
    void JoinConnection();

    //state machine
    void StateProcesser();
    void ProcessInit();
    void ProcessBuildSent();
    void ProcessListen();
    void ProcessJoinSent();
    void ProcessJoinRcvd();
    void ProcessConnected();
    void ProcessClosed();
    void ProcessError();

    //member accessor
    void SetPathId(MP_PATH_ID_t id);
    MP_PATH_ID_t GetPathId();
    void SetSocket(Ptr<Socket> sock);
    void SetServer(CybertwinDataTransferServer* server);
    MP_CONN_KEY_t GetLocalKey();
    void SetLocalKey(MP_CONN_KEY_t key);
    MP_CONN_KEY_t GetRemoteKey();
    void SetRemoteKey(MP_CONN_KEY_t key);
    void SetRemoteInteface(CYBERTWIN_INTERFACE_t remotIf);
    void SetPeerAddress(Address addr);
    void SetConnection(MultipathConnection* conn);
    void SetPathState(PathStatus stat);
    PathStatus GetPathState();
    void SetConnectionID(MP_CONN_ID_t id);
    MP_CONN_ID_t GetConnectionID();
    void SetLocalCybertwinID(CYBERTWINID_t id);
    CYBERTWINID_t GetLocalCybertwinID();
    void SetRemoteCybertwinID(CYBERTWINID_t id);
    CYBERTWINID_t GetRemoteCybertwinID();

    //
    int32_t SendPacketWithJoinTag(MultipathTagConn tag);
    //int32_t SendPacketWithDataTag(MultipathTagConn tag);
    int32_t SendPacketWithHeader(MultipathHeader header);
    
private:
    MP_PATH_ID_t m_pathId;
    Ptr<Socket> m_socket;
    MP_CONN_KEY_t m_localKey;
    MP_CONN_KEY_t m_remoteKey;
    CYBERTWINID_t m_localCybertwinID;
    CYBERTWIN_INTERFACE_t m_remoteIf;
    CYBERTWINID_t m_remoteCybertwinID;

    Address m_peerAddr;
    MultipathConnection *m_connection;
    CybertwinDataTransferServer *m_server;
    PathStatus m_pathState;
    MP_CONN_ID_t m_connID;

    MpDataSeqNum m_rxHeadSeqNum;

    // test data transfer
    std::queue<Ptr<Packet>> m_rxBuffer;
    Callback<void, SinglePath*> m_recvCallback;
};

#endif
