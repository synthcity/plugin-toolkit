use clack_extensions::audio_ports::{AudioPortInfoBuffer, PluginAudioPorts};
use clack_host::events::event_types::ParamValueEvent;
use clack_host::factory::plugin::PluginFactory;
use clack_host::prelude::*;
use clack_plugin::entry::SinglePluginEntry;
use portable_clack_gain::GainPlugin;

#[test]
pub fn sample_accurate_gain() {
    for rate in [44100., 48000., 96000.] {
        check_rate(rate);
    }
}

fn check_rate(rate: f64) {
    // Initialize host with basic info
    let info = HostInfo::new("test", "", "", "").unwrap();

    let entry = PluginEntry::load_from_clack::<SinglePluginEntry<GainPlugin>>(c"").unwrap();

    let descriptor = entry
        .get_factory::<PluginFactory>()
        .unwrap()
        .plugin_descriptor(0)
        .unwrap();

    assert_eq!(
        descriptor.id().unwrap().to_bytes(),
        b"org.example.portable-clack-gain"
    );
    assert_eq!(
        descriptor.name().unwrap().to_bytes(),
        b"Portable Clack Gain"
    );

    assert!(descriptor.vendor().is_none());
    assert!(descriptor.url().is_none());
    assert!(descriptor.manual_url().is_none());
    assert!(descriptor.support_url().is_none());
    assert!(descriptor.description().is_some());
    assert_eq!(descriptor.version().unwrap().to_bytes(), b"0.1.0");

    assert_eq!(
        descriptor
            .features()
            .map(|s| s.to_bytes())
            .collect::<Vec<_>>(),
        &[&b"audio-effect"[..], &b"stereo"[..]]
    );

    // Instantiate the desired plugin
    let mut plugin = PluginInstance::<TestHostHandlers>::new(
        |_| TestHostShared,
        |_| TestHostMainThread,
        &entry,
        descriptor.id().unwrap(),
        &info,
    )
    .unwrap();

    let plugin_main_thread = plugin.plugin_handle();
    let ports_ext = plugin_main_thread
        .get_extension::<PluginAudioPorts>()
        .unwrap();
    assert_eq!(1, ports_ext.count(&plugin_main_thread, true));
    assert_eq!(1, ports_ext.count(&plugin_main_thread, false));

    let mut buf = AudioPortInfoBuffer::new();
    let info = ports_ext
        .get(&plugin_main_thread, 0, false, &mut buf)
        .unwrap();

    assert_eq!(info.id, 0);
    assert_eq!(info.name, b"main");

    // Setting up some buffers
    let configuration = PluginAudioConfiguration {
        sample_rate: rate,
        min_frames_count: 32,
        max_frames_count: 32,
    };

    let processor = plugin
        .activate(|_, _| TestHostAudioProcessor, configuration)
        .unwrap();

    assert!(plugin.is_active());

    let mut input_events = EventBuffer::with_capacity(10);
    let mut output_events = EventBuffer::with_capacity(10);

    input_events.push(&ParamValueEvent::new(
        0,
        ClapId::new(1),
        Pckn::match_all(),
        1.0,
    ));

    input_events.push(&ParamValueEvent::new(
        16,
        ClapId::new(1),
        Pckn::match_all(),
        0.25,
    ));
    let mut input_buffers = [vec![0.8f32; 32], vec![-0.4f32; 32]];
    let mut output_buffers = [vec![0f32; 32], vec![0f32; 32]];

    let mut processor = processor.start_processing().unwrap();

    let mut inputs_descriptors = AudioPorts::with_capacity(2, 1);
    let mut outputs_descriptors = AudioPorts::with_capacity(2, 1);

    let input_channels = inputs_descriptors.with_input_buffers([AudioPortBuffer {
        channels: AudioPortBufferType::f32_input_only(
            input_buffers.iter_mut().map(InputChannel::variable),
        ),
        latency: 0,
    }]);

    let mut output_channels = outputs_descriptors.with_output_buffers([AudioPortBuffer {
        channels: AudioPortBufferType::f32_output_only(
            output_buffers.iter_mut().map(|b| b.as_mut_slice()),
        ),
        latency: 0,
    }]);

    processor
        .process(
            &input_channels,
            &mut output_channels,
            &input_events.as_input(),
            &mut output_events.as_output(),
            None,
            None,
        )
        .unwrap();

    // Check the gain was applied properly
    for channel_index in 0..2 {
        let inbuf = &input_buffers[channel_index];
        let outbuf = &output_buffers[channel_index];
        for (i, (input, output)) in inbuf.iter().zip(outbuf.iter()).enumerate() {
            assert_eq!(*output, *input * if i < 16 { 1.0 } else { 0.25 })
        }
    }

    plugin.deactivate(processor.stop_processing());
}

struct TestHostMainThread;
struct TestHostShared;
struct TestHostAudioProcessor;
struct TestHostHandlers;

impl SharedHandler<'_> for TestHostShared {
    fn request_restart(&self) {
        unimplemented!()
    }

    fn request_process(&self) {
        unimplemented!()
    }

    fn request_callback(&self) {
        unimplemented!()
    }
}

impl AudioProcessorHandler<'_> for TestHostAudioProcessor {}

impl MainThreadHandler<'_> for TestHostMainThread {}

impl HostHandlers for TestHostHandlers {
    type Shared<'a> = TestHostShared;
    type MainThread<'a> = TestHostMainThread;
    type AudioProcessor<'a> = TestHostAudioProcessor;
}
