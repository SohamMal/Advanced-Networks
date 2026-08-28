#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BGPPartE");

static Ipv4Address g_destAddr; ///< R5 address (sink)
static uint16_t    g_port;     ///< UDP port

//---
static void
OnOffTx (Ptr<const Packet> packet)
{
    std::cout << "t=" << Simulator::Now ().GetSeconds ()
              << "s  client sent " << packet->GetSize ()
              << " bytes to " << g_destAddr
              << " port " << g_port << "\n";
}

// -----
static void
SinkRx (Ptr<const Packet> packet, const Address& from)
{
    InetSocketAddress addr = InetSocketAddress::ConvertFrom (from);
    std::cout << "t=" << Simulator::Now ().GetSeconds ()
              << "s  server received " << packet->GetSize ()
              << " bytes from " << addr.GetIpv4 ()
              << " port " << addr.GetPort () << "\n";
}

int main (int argc, char* argv[])
{
    Time::SetResolution (Time::NS);

    CommandLine cmd;
    cmd.Parse (argc, argv);
    
    //---
    NodeContainer routers;
    routers.Create (5);

    //----
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
    posAlloc->Add (Vector (  0.0,  50.0, 0.0)); // R1
    posAlloc->Add (Vector ( 75.0, 100.0, 0.0)); // R2
    posAlloc->Add (Vector (150.0, 100.0, 0.0)); // R3
    posAlloc->Add (Vector ( 75.0,   0.0, 0.0)); // R4
    posAlloc->Add (Vector (225.0,  50.0, 0.0)); // R5
    mobility.SetPositionAllocator (posAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (routers);

    //---
    InternetStackHelper internet;
    internet.Install (routers);

    //---
    // R1 -- R2
    PointToPointHelper p2p12;
    p2p12.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
    p2p12.SetChannelAttribute ("Delay",    StringValue ("3ms"));
    NetDeviceContainer d12 = p2p12.Install (routers.Get (0), routers.Get (1));

    // R2 -- R3
    PointToPointHelper p2p23;
    p2p23.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
    p2p23.SetChannelAttribute ("Delay",    StringValue ("3ms"));
    NetDeviceContainer d23 = p2p23.Install (routers.Get (1), routers.Get (2));

    // R3 -- R5
    PointToPointHelper p2p35;
    p2p35.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
    p2p35.SetChannelAttribute ("Delay",    StringValue ("3ms"));
    NetDeviceContainer d35 = p2p35.Install (routers.Get (2), routers.Get (4));

    // R1 -- R4  (PRIMARY -- this is the link that will fail at t=6s)
    PointToPointHelper p2p14;
    p2p14.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
    p2p14.SetChannelAttribute ("Delay",    StringValue ("3ms"));
    NetDeviceContainer d14 = p2p14.Install (routers.Get (0), routers.Get (3));

    // R4 -- R5
    PointToPointHelper p2p45;
    p2p45.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
    p2p45.SetChannelAttribute ("Delay",    StringValue ("3ms"));
    NetDeviceContainer d45 = p2p45.Install (routers.Get (3), routers.Get (4));

    //---
    Ipv4AddressHelper ipv4;

    ipv4.SetBase ("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = ipv4.Assign (d12);

    ipv4.SetBase ("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = ipv4.Assign (d23);

    ipv4.SetBase ("10.3.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i35 = ipv4.Assign (d35);

    ipv4.SetBase ("10.4.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i14 = ipv4.Assign (d14);

    ipv4.SetBase ("10.5.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i45 = ipv4.Assign (d45);

    // Sink is R5's address reachable via Path B (10.5.0.2)
    g_destAddr = i45.GetAddress (1);
    g_port     = 8080;

    //---
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    std::cout << "Routing tables populated.\n" << "Default route R1->R5 uses Path B (R1->R4->R5, 2 hops).\n\n";

    // ----
    PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), g_port));
    ApplicationContainer serverApps = sinkHelper.Install (routers.Get (4));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop  (Seconds (14.0));

    //---
    OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (g_destAddr, g_port));
    onoff.SetConstantRate (DataRate ("500Kbps"), 512);
    onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e9]"));
    onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer clientApps = onoff.Install (routers.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop  (Seconds (14.0));

    //---
    clientApps.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&OnOffTx));
    serverApps.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&SinkRx));

    // -----
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    // ------
    AnimationInterface anim ("bgp_partE_animation.xml");
    anim.SetConstantPosition (routers.Get (0),   0.0,  50.0);
    anim.SetConstantPosition (routers.Get (1),  75.0, 100.0);
    anim.SetConstantPosition (routers.Get (2), 150.0, 100.0);
    anim.SetConstantPosition (routers.Get (3),  75.0,   0.0);
    anim.SetConstantPosition (routers.Get (4), 225.0,  50.0);

    // -----
    Simulator::Schedule (Seconds (6.0), [&] ()
    {
        std::cout << "\n"
                  << "=============================================\n"
                  << "  t=6s : R1-R4 LINK FAILURE\n"
                  << "  Path B DOWN -- rerouting to Path A\n"
                  << "=============================================\n\n";

        Ptr<Ipv4> r1Ipv4 = routers.Get (0)->GetObject<Ipv4> ();
        Ptr<Ipv4> r4Ipv4 = routers.Get (3)->GetObject<Ipv4> ();

        uint32_t r1Iface = r1Ipv4->GetInterfaceForDevice (d14.Get (0));
        uint32_t r4Iface = r4Ipv4->GetInterfaceForDevice (d14.Get (1));

        r1Ipv4->SetDown (r1Iface);
        r4Ipv4->SetDown (r4Iface);

        Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();
    });

    // -----
    Simulator::Schedule (Seconds (9.0), [&] ()
    {
        std::cout << "\n"
                  << "=============================================\n"
                  << "  t=9s : R1-R4 LINK RECOVERY\n"
                  << "  Path B restored -- returning to primary\n"
                  << "=============================================\n\n";

        Ptr<Ipv4> r1Ipv4 = routers.Get (0)->GetObject<Ipv4> ();
        Ptr<Ipv4> r4Ipv4 = routers.Get (3)->GetObject<Ipv4> ();

        uint32_t r1Iface = r1Ipv4->GetInterfaceForDevice (d14.Get (0));
        uint32_t r4Iface = r4Ipv4->GetInterfaceForDevice (d14.Get (1));

        r1Ipv4->SetUp (r1Iface);
        r4Ipv4->SetUp (r4Iface);

        Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();
    });

    //--
    Simulator::Stop (Seconds (14.0));
    Simulator::Run ();

    // -------
    monitor->CheckForLostPackets ();
    monitor->SerializeToXmlFile ("bgp_partE_flowmon.xml", true, true);

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    std::cout << "\n=============================================\n"
              << "          FINAL FLOW STATISTICS\n"
              << "=============================================\n";

    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);

        if (t.destinationAddress == g_destAddr)
        {
            std::cout << "\n  Flow: R1 -> R5  ("
                      << t.sourceAddress << " -> " << t.destinationAddress << ")\n"
                      << "  Tx Packets   : " << flow.second.txPackets   << "\n"
                      << "  Rx Packets   : " << flow.second.rxPackets   << "\n"
                      << "  Lost Packets : " << flow.second.lostPackets << "\n";

            if (flow.second.rxPackets > 0)
            {
                double avgDelay = flow.second.delaySum.GetSeconds () / flow.second.rxPackets;
                std::cout << "  Avg Delay    : " << avgDelay * 1000.0 << " ms\n";
            }

            if (flow.second.rxPackets > 1)
            {
                double avgJitter = flow.second.jitterSum.GetSeconds () / (flow.second.rxPackets - 1);
                std::cout << "  Avg Jitter   : " << avgJitter * 1000.0 << " ms\n";
            }
        }
    }

    std::cout << "\n=============================================\n\n";

    Simulator::Destroy ();
    return 0;
}
