// Tests for the scene syncing in Source/Plugins/SceneSync.h
// Build and run: tests/preset-scenes/run.sh

#include <juce_core/juce_core.h>

using namespace juce;

#include "../../Source/Plugins/SceneSync.h"

#include <cstdio>

using namespace Curve::Scenes;

static int failures = 0;

#define CHECK(condition) \
    do { if (! (condition)) { std::printf ("FAILED line %d: %s\n", __LINE__, #condition); ++failures; } } while (false)

static std::unique_ptr<XmlElement> parse (const char* text)
{
    auto xml = parseXML (String (text));
    jassert (xml != nullptr);
    return xml;
}

static const XmlElement* node (const XmlElement& graph, int uid)
{
    for (auto* n : graph.getChildWithTagNameIterator ("FILTER"))
        if (n->getIntAttribute ("uid") == uid)
            return n;

    return nullptr;
}

static String state (const XmlElement& graph, int uid)
{
    auto* n = node (graph, uid);
    auto* s = n != nullptr ? n->getChildByName ("STATE") : nullptr;
    return s != nullptr ? s->getAllSubText() : String();
}

static bool bypassed (const XmlElement& graph, int uid)
{
    auto* n = node (graph, uid);
    return n != nullptr && n->getBoolAttribute ("bypassed");
}

// Studio: EQ (2) is bypassed; the crossfeed (3) varies by scene
static const char* studio = R"(
<FILTERGRAPH sceneGroup="g1">
  <FILTER uid="1" x="0.5" y="0.1"><STATE>io</STATE></FILTER>
  <FILTER uid="2" x="0.5" y="0.4" bypassed="1"><STATE>eq-A</STATE></FILTER>
  <FILTER uid="3" x="0.5" y="0.6" varyByScene="1"><STATE>xfeed-studio</STATE></FILTER>
  <CONNECTION srcFilter="1" srcChannel="0" dstFilter="2" dstChannel="0"/>
</FILTERGRAPH>)";

static const char* late = R"(
<FILTERGRAPH sceneGroup="g1">
  <FILTER uid="1" x="0.5" y="0.1"><STATE>io</STATE></FILTER>
  <FILTER uid="2" x="0.5" y="0.4"><STATE>eq-A</STATE></FILTER>
  <FILTER uid="3" x="0.5" y="0.6" varyByScene="1"><STATE>xfeed-late</STATE></FILTER>
  <CONNECTION srcFilter="1" srcChannel="0" dstFilter="2" dstChannel="0"/>
</FILTERGRAPH>)";

static void testScenesShareGraph()
{
    CHECK (shareGraph (*parse (studio), *parse (late)));
}

static void testDifferentSharedStateDoesNotShare()
{
    auto other = parse (late);
    node (*other, 2)->getChildByName ("STATE")->getFirstChildElement()->setText ("eq-B");
    CHECK (! shareGraph (*parse (studio), *other));
}

static void testDifferentConnectionsDoNotShare()
{
    auto other = parse (late);
    other->removeChildElement (other->getChildByName ("CONNECTION"), true);
    CHECK (! shareGraph (*parse (studio), *other));
}

static void testSharedChangeReachesOtherScene()
{
    // Saved from Studio after changing the EQ, a shared node
    auto saved = parse (studio);
    node (*saved, 2)->getChildByName ("STATE")->getFirstChildElement()->setText ("eq-B");

    const auto result = createOtherScene (*saved, *parse (late));
    CHECK (state (*result, 2) == "eq-B");
    CHECK (! bypassed (*result, 2));                  // Late's own bypass
    CHECK (state (*result, 3) == "xfeed-late");       // Late's own crossfeed
    CHECK (result->getStringAttribute ("sceneGroup") == "g1");
    CHECK (shareGraph (*saved, *result));
}

static void testNewNodeIsBypassedInOtherScenes()
{
    auto saved = parse (studio);
    auto* added = saved->createNewChildElement ("FILTER");
    added->setAttribute ("uid", 4);
    added->createNewChildElement ("STATE")->addTextElement ("limiter");

    const auto result = createOtherScene (*saved, *parse (late));
    CHECK (bypassed (*result, 4));
    CHECK (state (*result, 4) == "limiter");
    CHECK (! bypassed (*saved, 4));
}

static void testRemovedNodeIsRemovedEverywhere()
{
    auto saved = parse (studio);
    saved->removeChildElement (const_cast<XmlElement*> (node (*saved, 3)), true);

    const auto result = createOtherScene (*saved, *parse (late));
    CHECK (node (*result, 3) == nullptr);
}

static void testNewlyVaryingNodeKeepsEachScenesState()
{
    // Studio marks the EQ as varying and changes it: Late keeps the EQ it had
    auto saved = parse (studio);
    auto* eq = const_cast<XmlElement*> (node (*saved, 2));
    eq->setAttribute ("varyByScene", true);
    eq->getChildByName ("STATE")->getFirstChildElement()->setText ("eq-studio");

    const auto result = createOtherScene (*saved, *parse (late));
    CHECK (state (*result, 2) == "eq-A");
    CHECK (node (*result, 2)->getBoolAttribute ("varyByScene"));
}

static void testNoLongerVaryingNodeTakesSavedState()
{
    auto saved = parse (studio);
    const_cast<XmlElement*> (node (*saved, 3))->removeAttribute ("varyByScene");

    const auto result = createOtherScene (*saved, *parse (late));
    CHECK (state (*result, 3) == "xfeed-studio");
}

static void testRoundTripThroughFileText()
{
    // The check on saving compares a file read back with one read from disk
    auto saved = parse (studio);
    auto reread = parseXML (saved->toString());
    CHECK (reread != nullptr && shareGraph (*saved, *reread));
}

static void testBypassOnlyChangesNothingShared()
{
    // Saved from Studio with the EQ turned on: Late keeps its own bypass, so nothing to warn of
    auto saved = parse (studio);
    const_cast<XmlElement*> (node (*saved, 2))->removeAttribute ("bypassed");

    const auto changes = describeChanges (*saved, *parse (late));
    CHECK (! changes.changesSound());
    CHECK (changes.addedNodes.isEmpty());
}

static void testVaryingStateIsNotAChange()
{
    auto saved = parse (studio);
    node (*saved, 3)->getChildByName ("STATE")->getFirstChildElement()->setText ("xfeed-new");

    CHECK (! describeChanges (*saved, *parse (late)).changesSound());
}

static void testSharedSettingChangeIsNamed()
{
    auto saved = parse (studio);
    auto* eq = const_cast<XmlElement*> (node (*saved, 2));
    eq->setAttribute ("customNodeName", "Room EQ");
    eq->getChildByName ("STATE")->getFirstChildElement()->setText ("eq-B");

    const auto changes = describeChanges (*saved, *parse (late));
    CHECK (changes.changedNodes == StringArray ("Room EQ"));
    CHECK (! changes.connectionsChanged);
}

static void testAddedRemovedAndConnectionsAreDescribed()
{
    auto saved = parse (studio);
    saved->removeChildElement (const_cast<XmlElement*> (node (*saved, 3)), true);
    saved->removeChildElement (saved->getChildByName ("CONNECTION"), true);
    auto* added = saved->createNewChildElement ("FILTER");
    added->setAttribute ("uid", 4);
    added->createNewChildElement ("PLUGIN")->setAttribute ("name", "Limiter");

    const auto changes = describeChanges (*saved, *parse (late));
    CHECK (changes.addedNodes == StringArray ("Limiter"));
    CHECK (changes.removedNodes == StringArray ("Node 3"));
    CHECK (changes.connectionsChanged);
    CHECK (changes.changesSound());
}

static void testMovingANodeIsNotAChange()
{
    auto saved = parse (studio);
    const_cast<XmlElement*> (node (*saved, 2))->setAttribute ("x", "0.9");

    CHECK (! describeChanges (*saved, *parse (late)).changesSound());
}

static void testPluginDescriptionTimesAreIgnored()
{
    // A plug-in's description gets a new infoUpdateTime each time it's created, i.e. each load
    auto withTime = [] (const char* graph, const char* time)
    {
        auto xml = parse (graph);
        auto* plugin = const_cast<XmlElement*> (node (*xml, 2))->createNewChildElement ("PLUGIN");
        plugin->setAttribute ("name", "AUNBandEQ");
        plugin->setAttribute ("infoUpdateTime", time);
        return xml;
    };

    const auto saved = withTime (studio, "1a1dbf65d96");
    const auto other = withTime (late, "1a190114a49");

    CHECK (shareGraph (*saved, *other));
    CHECK (! describeChanges (*saved, *other).changesSound());
}

static void testVaryingNodeWithoutStateKeepsNone()
{
    // Late's crossfeed has no STATE, e.g. a file edited by hand: it doesn't get Studio's
    auto other = parse (late);
    auto* xfeed = const_cast<XmlElement*> (node (*other, 3));
    xfeed->removeChildElement (xfeed->getChildByName ("STATE"), true);

    const auto result = createOtherScene (*parse (studio), *other);
    CHECK (node (*result, 3)->getChildByName ("STATE") == nullptr);
}

static void testVaryingNodeGetsOwnStateWhenSavedHasNone()
{
    auto saved = parse (studio);
    auto* xfeed = const_cast<XmlElement*> (node (*saved, 3));
    xfeed->removeChildElement (xfeed->getChildByName ("STATE"), true);

    const auto result = createOtherScene (*saved, *parse (late));
    CHECK (state (*result, 3) == "xfeed-late");
}

// Audio Output saved with a 14-channel interface in one scene and 2-channel headphones in the
// other: its channel count is the interface's, so it's neither shared nor a change
static const char* withOutput (const char* numInputs, const char* layout, const char* extraConnection)
{
    static String text;
    text = String (R"(
<FILTERGRAPH sceneGroup="g2">
  <FILTER uid="9" x="0.5" y="0.9">
    <PLUGIN name="Audio Output" format="Internal" file="" uniqueId="724248cb" numInputs=")") + numInputs
         + R"(" numOutputs="0"/>
    <STATE>out</STATE>
    <LAYOUT><INPUTS><BUS index="0" layout=")" + layout + R"("/></INPUTS></LAYOUT>
  </FILTER>
  <FILTER uid="2" x="0.5" y="0.4"><STATE>eq-A</STATE></FILTER>
  <CONNECTION srcFilter="2" srcChannel="0" dstFilter="9" dstChannel="0"/>
  <CONNECTION srcFilter="2" srcChannel="1" dstFilter="9" dstChannel="3"/>
  )" + extraConnection + "\n</FILTERGRAPH>";
    return text.toRawUTF8();
}

static void testAudioDeviceChannelsAreEachScenes()
{
    auto onInterface  = parse (withOutput ("14", "14", ""));
    auto onHeadphones = parse (withOutput ("2", "L R", ""));

    CHECK (shareGraph (*onInterface, *onHeadphones));
    CHECK (! describeChanges (*onHeadphones, *onInterface).changesSound());

    auto updated = createOtherScene (*onHeadphones, *onInterface);
    auto* output = node (*updated, 9);
    CHECK (output->getChildByName ("PLUGIN")->getStringAttribute ("numInputs") == "14");
    CHECK (output->getChildByName ("LAYOUT")->getChildByName ("INPUTS")->getChildByName ("BUS")->getStringAttribute ("layout") == "14");

    // Connections to it are still shared, including to outputs the headphones don't have
    CHECK (getConnections (*updated) == getConnections (*onHeadphones));
}

static void testConnectionToAudioDeviceIsStillAChange()
{
    auto before = parse (withOutput ("14", "14", ""));
    auto after  = parse (withOutput ("2", "L R", R"(<CONNECTION srcFilter="2" srcChannel="0" dstFilter="9" dstChannel="2"/>)"));

    CHECK (! shareGraph (*before, *after));
    CHECK (describeChanges (*after, *before).connectionsChanged);
}

// Opening or moving a plug-in window in one scene isn't a change to the others, which keep their own
static void testPluginWindowsAreEachScenes()
{
    auto early = parse (R"(<FILTERGRAPH sceneGroup="g">
  <FILTER uid="2" x="0.5" y="0.4" uiopen_Normal="1" uiLastX_Normal="10" uiLastY_Normal="20"><STATE>eq</STATE></FILTER>
</FILTERGRAPH>)");
    auto late = parse (R"(<FILTERGRAPH sceneGroup="g">
  <FILTER uid="2" x="0.5" y="0.4" uiopen_Normal="0" uiLastX_Normal="300" uiLastY_Normal="400"><STATE>eq</STATE></FILTER>
</FILTERGRAPH>)");

    CHECK (shareGraph (*early, *late));

    auto updated = createOtherScene (*early, *late);
    CHECK (node (*updated, 2)->getStringAttribute ("uiopen_Normal") == "0");
    CHECK (node (*updated, 2)->getStringAttribute ("uiLastX_Normal") == "300");

    // Nor one that the other scene has no position for
    auto noWindow = parse (R"(<FILTERGRAPH sceneGroup="g"><FILTER uid="2" x="0.5" y="0.4"><STATE>eq</STATE></FILTER></FILTERGRAPH>)");
    CHECK (! createOtherScene (*early, *noWindow)->getChildByName ("FILTER")->hasAttribute ("uiLastX_Normal"));
}

// I/O nodes and meters can't be bypassed, so are added to the other scenes as they are
static void testNewIONodeIsNotBypassed()
{
    auto other = parse (R"(<FILTERGRAPH sceneGroup="g"><FILTER uid="2" x="0.5" y="0.4"><STATE>eq</STATE></FILTER></FILTERGRAPH>)");
    auto saved = parse (R"(<FILTERGRAPH sceneGroup="g">
  <FILTER uid="2" x="0.5" y="0.4"><STATE>eq</STATE></FILTER>
  <FILTER uid="7" x="0.1" y="0.1"><PLUGIN name="Meter" format="Internal" file="Meter"/></FILTER>
  <FILTER uid="8" x="0.9" y="0.9"><PLUGIN name="Audio Output" format="Internal" file="Audio Output"/></FILTER>
  <FILTER uid="9" x="0.9" y="0.1"><PLUGIN name="Crossfeed" format="Internal" file="Crossfeed"/></FILTER>
</FILTERGRAPH>)");

    auto updated = createOtherScene (*saved, *other);
    CHECK (! bypassed (*updated, 7));
    CHECK (! bypassed (*updated, 8));
    CHECK (bypassed (*updated, 9));

    const auto changes = describeChanges (*saved, *other);
    CHECK (changes.addedIONodes.size() == 2);
    CHECK (changes.addedNodes.size() == 1);
}

// MIDI mappings are shared, so a change to them is described
static void testMidiMappingChangeIsDescribed()
{
    auto other = parse (R"(<FILTERGRAPH sceneGroup="g"><FILTER uid="2" x="0.5" y="0.4"><STATE>eq</STATE></FILTER></FILTERGRAPH>)");
    auto saved = parse (R"(<FILTERGRAPH sceneGroup="g"><FILTER uid="2" x="0.5" y="0.4"><STATE>eq</STATE></FILTER>
  <MIDIMAPPINGS><MAPPING type="cc" channel="1" number="7"/></MIDIMAPPINGS></FILTERGRAPH>)");

    CHECK (describeChanges (*saved, *other).midiMappingsChanged);
    CHECK (! describeChanges (*saved, *createOtherScene (*saved, *other)).midiMappingsChanged);
    CHECK (! describeChanges (*saved, *other).changesSound());
}

int main()
{
    testVaryingNodeWithoutStateKeepsNone();
    testVaryingNodeGetsOwnStateWhenSavedHasNone();
    testPluginDescriptionTimesAreIgnored();
    testBypassOnlyChangesNothingShared();
    testVaryingStateIsNotAChange();
    testSharedSettingChangeIsNamed();
    testAddedRemovedAndConnectionsAreDescribed();
    testMovingANodeIsNotAChange();
    testScenesShareGraph();
    testDifferentSharedStateDoesNotShare();
    testDifferentConnectionsDoNotShare();
    testSharedChangeReachesOtherScene();
    testNewNodeIsBypassedInOtherScenes();
    testRemovedNodeIsRemovedEverywhere();
    testNewlyVaryingNodeKeepsEachScenesState();
    testNoLongerVaryingNodeTakesSavedState();
    testRoundTripThroughFileText();
    testAudioDeviceChannelsAreEachScenes();
    testConnectionToAudioDeviceIsStillAChange();
    testPluginWindowsAreEachScenes();
    testNewIONodeIsNotBypassed();
    testMidiMappingChangeIsDescribed();

    if (failures == 0)
        std::printf ("All preset scene tests passed\n");

    return failures == 0 ? 0 : 1;
}
