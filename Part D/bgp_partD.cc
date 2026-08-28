#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("InterASRoutingLite");

//----
static std::vector<Time> g_txTimes;

static std::ofstream g_delayCsv;

static void ClientTx (Ptr<const Packet> packet)
{
  g_txTimes.push_back (Simulator::Now ());
}

static void ClientRx (Ptr<const Packet> packet)
{
  if (g_txTimes.empty ())
    {
      return;
    }

  Time txTime = g_txTimes.front ();
  g_txTimes.erase (g_txTimes.begin ());

  Time rxTime = Simulator::Now ();

  double delayMs = (rxTime - txTime).GetMilliSeconds ();

  double timeSeconds = rxTime.GetSeconds ();

  g_delayCsv << timeSeconds << "," << delayMs << "\n";
}

//----
int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  CommandLine cmd;
  cmd.Parse (argc, argv);

//----
  LogComponentEnable ( "InterASRoutingLite", LOG_LEVEL_INFO);

  LogComponentEnable ( "UdpEchoClientApplication", LOG_LEVEL_INFO);

  LogComponentEnable ( "UdpEchoServerApplication", LOG_LEVEL_INFO);

  NS_LOG_INFO ("Starting Inter-AS routing simulation with 4 routers " "(AS1-AS2-AS3-AS4)...");


//----
  NodeContainer routers;
  routers.Create (4);

//----
  MobilityHelper mobility;

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

  positionAlloc->Add (Vector (0.0, 50.0, 0.0));       // AS1

  positionAlloc->Add (Vector (50.0, 100.0, 0.0));     // AS2

  positionAlloc->Add (Vector (100.0, 100.0, 0.0));    // AS3

  positionAlloc->Add (Vector (150.0, 50.0, 0.0));     // AS4

  mobility.SetPositionAllocator (positionAlloc);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (routers);


//----
  InternetStackHelper internet;
  internet.Install (routers);


//----
  PointToPointHelper p2p12;

  p2p12.SetDeviceAttribute ("DataRate", StringValue ("8Mbps"));

  p2p12.SetChannelAttribute ("Delay", StringValue ("3ms"));

  NetDeviceContainer d12 = p2p12.Install (routers.Get (0), routers.Get (1));


  PointToPointHelper p2p23;

  p2p23.SetDeviceAttribute ( "DataRate", StringValue ("6Mbps"));

  p2p23.SetChannelAttribute ( "Delay", StringValue ("4ms"));

  NetDeviceContainer d23 = p2p23.Install (routers.Get (1), routers.Get (2));


  PointToPointHelper p2p34;

  p2p34.SetDeviceAttribute ("DataRate", StringValue ("8Mbps"));

  p2p34.SetChannelAttribute ("Delay", StringValue ("3ms"));

  NetDeviceContainer d34 = p2p34.Install (routers.Get (2), routers.Get (3));
  

  PointToPointHelper p1p4;

  p1p4.SetDeviceAttribute ("DataRate", StringValue ("11Mbps"));

  p1p4.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer d14 = p1p4.Install (routers.Get (0), routers.Get (3));


//----
  Ipv4AddressHelper ipv4;

  // AS1 -- AS2
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (d12);

  // AS2 -- AS3
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign (d23);

  // AS3 -- AS4
  ipv4.SetBase ("192.168.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i34 = ipv4.Assign (d34);

  // AS1 -- AS4
  ipv4.SetBase ("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i14 = ipv4.Assign (d14);


//----
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  uint16_t port = 8080;
  
//----
  UdpEchoServerHelper server (port);

  ApplicationContainer serverApps = server.Install (routers.Get (3));

  serverApps.Start ( Seconds (1.0));

  serverApps.Stop ( Seconds (14.0));

//---
  UdpEchoClientHelper client (i34.GetAddress (1), port);

  client.SetAttribute ("MaxPackets", UintegerValue (16));

  client.SetAttribute ( "Interval", TimeValue (Seconds (0.75)));

  client.SetAttribute ( "PacketSize", UintegerValue (512));

  ApplicationContainer clientApps = client.Install (routers.Get (0));

  clientApps.Start (Seconds (2.0));

  clientApps.Stop (Seconds (14.0));


//-----
  clientApps.Get (0)->TraceConnectWithoutContext (
    "Tx",
    MakeCallback (&ClientTx));

  clientApps.Get (0)->TraceConnectWithoutContext (
    "Rx",
    MakeCallback (&ClientRx));

//----
  FlowMonitorHelper flowmon;

  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

//----
  AnimationInterface anim ("bgp_partD_animation.xml");


//----
  g_delayCsv.open ("bgp_partD_delay.csv");

  g_delayCsv << "time,delay_ms\n";


//----
  Simulator::Schedule ( Seconds (6.0), [&] ()
    {
      std::cout
        << "\n\n"
        << "=============================================\n"
        << "At time +6s: AS1-AS4 LINK FAILURE\n"
        << "=============================================\n";

      Ptr<Ipv4> as1Ipv4 = routers.Get (0)->GetObject<Ipv4> ();

      Ptr<Ipv4> as4Ipv4 = routers.Get (3)->GetObject<Ipv4> ();

      uint32_t as1ToAs4Interface = as1Ipv4->GetInterfaceForDevice (d14.Get (0));

      uint32_t as4ToAs1Interface = as4Ipv4->GetInterfaceForDevice ( d14.Get (1));

      as1Ipv4->SetDown (as1ToAs4Interface);

      Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();

      std::cout << "AS1-AS4 direct link is DOWN.\n";

      std::cout << "Expected alternate path:\n";

      std::cout << "AS1 -> AS2 -> AS3 -> AS4\n";
    });


//-----
  Simulator::Schedule (Seconds (9.0),[&] ()
    {
      std::cout
        << "\n\n"
        << "=============================================\n"
        << "At time +9s: AS1-AS4 LINK RECOVERY\n"
        << "=============================================\n";

      Ptr<Ipv4> as1Ipv4 = routers.Get (0)->GetObject<Ipv4> ();

      Ptr<Ipv4> as4Ipv4 = routers.Get (3)->GetObject<Ipv4> ();

      uint32_t as1ToAs4Interface = as1Ipv4->GetInterfaceForDevice (d14.Get (0));

      uint32_t as4ToAs1Interface = as4Ipv4->GetInterfaceForDevice (d14.Get (1));

      as1Ipv4->SetUp (as1ToAs4Interface);

      as4Ipv4->SetUp (as4ToAs1Interface);

      Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();

      std::cout<< "AS1-AS4 direct link is UP.\n";

      std::cout<< "Expected restored path:\n";

      std::cout<< "AS1 -> AS4\n";
    });


//----

  Simulator::Stop (
    Seconds (14.0));

  Simulator::Run ();

//---
  monitor->CheckForLostPackets ();

  monitor->SerializeToXmlFile ( "interas-baseline-results_D.xml", true, true);


//----
  g_delayCsv.close ();

//----
  Simulator::Destroy ();

  return 0;
}
