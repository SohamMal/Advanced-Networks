#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("InterASRoutingLiteAnimation");

int
main(int argc, char* argv[])
{
    Time::SetResolution(Time::NS);
    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponentEnable("InterASRoutingLiteAnimation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NS_LOG_INFO("Starting inter-AS routing animation with AS1-AS2-AS3-AS4 and AS1-AS4 links...");

    NodeContainer routers;
    routers.Create(4);

    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(20.0, 50.0, 0.0));   // AS1
    positions->Add(Vector(70.0, 50.0, 0.0));   // AS2
    positions->Add(Vector(120.0, 50.0, 0.0));  // AS3
    positions->Add(Vector(20.0, 110.0, 0.0));  // AS4

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(routers);

    InternetStackHelper internet;
    internet.Install(routers);

    PointToPointHelper p2p12;
    p2p12.SetDeviceAttribute("DataRate", StringValue("8Mbps"));
    p2p12.SetChannelAttribute("Delay", StringValue("3ms"));
    NetDeviceContainer d12 = p2p12.Install(routers.Get(0), routers.Get(1));

    PointToPointHelper p2p23;
    p2p23.SetDeviceAttribute("DataRate", StringValue("6Mbps"));
    p2p23.SetChannelAttribute("Delay", StringValue("4ms"));
    NetDeviceContainer d23 = p2p23.Install(routers.Get(1), routers.Get(2));

    PointToPointHelper p2p34;
    p2p34.SetDeviceAttribute("DataRate", StringValue("8Mbps"));
    p2p34.SetChannelAttribute("Delay", StringValue("3ms"));
    NetDeviceContainer d34 = p2p34.Install(routers.Get(2), routers.Get(3));

    PointToPointHelper p2p14;
    p2p14.SetDeviceAttribute("DataRate", StringValue("11Mbps"));
    p2p14.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer d14 = p2p14.Install(routers.Get(0), routers.Get(3));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = ipv4.Assign(d12);

    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = ipv4.Assign(d23);

    ipv4.SetBase("192.168.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i34 = ipv4.Assign(d34);

    ipv4.SetBase("192.168.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i14 = ipv4.Assign(d14);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4> as1Ipv4 = routers.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> as1StaticRouting = staticRoutingHelper.GetStaticRouting(as1Ipv4);
    uint32_t as1ToAs2Interface = as1Ipv4->GetInterfaceForDevice(d12.Get(0));
    as1StaticRouting->AddHostRouteTo(i34.GetAddress(1), i12.GetAddress(1), as1ToAs2Interface);

    Ptr<Ipv4> as2Ipv4 = routers.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> as2StaticRouting = staticRoutingHelper.GetStaticRouting(as2Ipv4);
    uint32_t as2ToAs3Interface = as2Ipv4->GetInterfaceForDevice(d23.Get(0));
    as2StaticRouting->AddHostRouteTo(i34.GetAddress(1), i23.GetAddress(1), as2ToAs3Interface);

    uint16_t port = 8080;
    UdpEchoServerHelper server(port);
    ApplicationContainer apps = server.Install(routers.Get(3));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(13.0));

    UdpEchoClientHelper client(i34.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(8));
    client.SetAttribute("Interval", TimeValue(Seconds(0.75)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    apps = client.Install(routers.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(13.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    AnimationInterface anim("bgp_partC1_animation.xml");
    
    Simulator::Stop(Seconds(14.0));
    Simulator::Run();

    monitor->SerializeToXmlFile("interas-baseline-results_C1.xml", true, true);

    Simulator::Destroy();
    return 0;
}
