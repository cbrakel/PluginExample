/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CBExamplePluginAudioProcessor::CBExamplePluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

CBExamplePluginAudioProcessor::~CBExamplePluginAudioProcessor()
{
}

//==============================================================================
const juce::String CBExamplePluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CBExamplePluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool CBExamplePluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool CBExamplePluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double CBExamplePluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int CBExamplePluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int CBExamplePluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void CBExamplePluginAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String CBExamplePluginAudioProcessor::getProgramName (int index)
{
    return {};
}

void CBExamplePluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void CBExamplePluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    juce::dsp::ProcessSpec spec;

    spec.maximumBlockSize = samplesPerBlock;

    spec.numChannels = 1;

    spec.sampleRate = sampleRate;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    auto chainSettings = getChainSettings(apvts);
    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                                        sampleRate,
                                                                                                        4);

    // Get LowCut filter chain
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();

    // Bypass all links in the chain
    leftLowCut.setBypassed<0>(true);
    leftLowCut.setBypassed<1>(true);
    leftLowCut.setBypassed<2>(true);
    leftLowCut.setBypassed<3>(true);

    leftLowCut.get<0>().coefficients = *cutCoefficients[0];
    leftLowCut.setBypassed<0>(false);
}

void CBExamplePluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CBExamplePluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void CBExamplePluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());


    auto chainSettings = getChainSettings(apvts);
    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
        getSampleRate(),
        4);

    // Get LowCut filter chain
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();

    // Bypass all links in the chain
    leftLowCut.setBypassed<0>(true);
    leftLowCut.setBypassed<1>(true);
    leftLowCut.setBypassed<2>(true);
    leftLowCut.setBypassed<3>(true);

    leftLowCut.get<0>().coefficients = *cutCoefficients[0];
    leftLowCut.setBypassed<0>(false);

    // Create an audio block
    juce::dsp::AudioBlock<float> block(buffer);

    // extract L and R channels from block
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    // create processing contexts that wrap each audio block
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    // pass these contexts to our chain
    leftChain.process(leftContext);
    rightChain.process(rightContext);
}

//==============================================================================
bool CBExamplePluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* CBExamplePluginAudioProcessor::createEditor()
{
    // return new CBExamplePluginAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void CBExamplePluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void CBExamplePluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;

    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();

    return settings;
}

juce::AudioProcessorValueTreeState::ParameterLayout
CBExamplePluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    //Parameter name = lowcut freq
    // Normalisable range, human hearing range is 20hz to 20 000 hz
        // That defines teh range, interval is one to control the interval of sliding
            // Higher ranges we could probably do a higher interval since the change is less noticable, but with lower frequencies a going up or down a few hurtz will be a semi tone difference which you can clearly hear
        // skew,  weights the slider ex maybe most of the mouse movements will focus on a specific frequency (one means no skew)
    // default - 20hz since this is low cut - don't change it until the slider moves
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq",
        "LowCut Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f),
        20.f));

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CBExamplePluginAudioProcessor();
}
