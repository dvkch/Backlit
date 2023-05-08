#!/usr/bin/env ruby

require "json"
require "colorize"
require "pathname"

def log(message)
    puts "💥 " + message.yellow
end

class Recorder
    attr_accessor :stop_everything
    def initialize
        @stop_everything = false
        @simulators = Hash.new
        @recordings = Hash.new
    end

    def observe
        Thread.new do
            log "Starting observing simulators"
            while !stop_everything
                refresh_simulators
                sleep 1
            end
            self.simulators = Hash.new
            log "Ended observing simulators"
            sleep 1
        end
    end

    def refresh_simulators
        output = JSON.parse(`xcrun simctl list -j`)
        self.simulators = output['devices']
            .values.flatten
            .filter { |d| d["state"] == "Booted" }
            .filter { |d| pid_in_simulator(d["udid"], "SaneScanner") != nil }
            .map { |d| [d["udid"], d["name"]] }
            .to_h
    end

    def pid_in_simulator(udid, process)
        pid = `ps x | grep -e 'Devices/#{udid}.*#{process}.app/#{process}' | grep -v "grep" | awk '{ print $1 }'`.strip
        pid.empty? ? nil : pid
    end

    attr_accessor :simulators
    def simulators=(value)
        started_udids = (value.keys - @simulators.keys)
        stopped_udids = (@simulators.keys - value.keys)

        @simulators = value

        stopped_udids.each do |udid|
            stop_recording(udid)
        end
        started_udids.each do |udid|
            start_recording(udid)
        end
    end

    attr_accessor :recordings
    def start_recording(udid)
        locale = simulator_locale(udid)
        dir = "#{Dir.pwd}/fastlane/recordings/#{locale}"
        `mkdir -p "#{dir}"`

        name = simulators[udid] || udid
        filename = "#{dir}/#{name}.mov"
        command = "xcrun simctl io #{udid} recordVideo -f \"#{filename}\""

        log "Starting recording for #{name} #{locale} (#{udid})"
        pid = spawn(command)
        Process.detach(pid)

        self.recordings[udid] = { pid: pid, filename: filename }
    end

    def stop_recording(udid)
        recording = recordings[udid]
        return if recording.nil?

        log "Stopping recording for #{udid} (pid=#{recording[:pid]})"
        `kill -INT #{recording[:pid]}`
        self.recordings.delete(udid)
    end

    def simulator_locale(udid)
        locale = `plutil -extract "AppleLanguages".0 raw -o - ~/Library/Developer/CoreSimulator/Devices/#{udid}/data/Library/Preferences/.GlobalPreferences.plist`
        locale = locale.strip
        locale = "en-US" if locale.nil? || locale.empty?
        locale
    end

    TRIM_BEFORE = 1.to_f
    TRIM_AFTER = 3.to_f
    def cleanup_videos
        log "Cleaning up videos"

        videos = Dir.glob("#{Dir.pwd}/fastlane/recordings/**/*.mov")
        videos.each do |original_path|
            next if File.basename(original_path).include?('-cut')
            next if File.basename(original_path).include?('-cleaned')

            puts "Cleaning up #{original_path.gsub(Dir.pwd + '/fastlane/recordings/', '')}"

            original_path_no_ext = Pathname(original_path).sub_ext('').to_s

            # get duration
            duration = `ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "#{original_path}"`.to_f

            # trim
            path_cut = original_path_no_ext + "-cut.mov"
            t_start = TRIM_BEFORE
            t_end = duration - TRIM_AFTER 

            opts_cut        = "-ss #{t_start} -to #{t_end}"
            opts_encoding   = "-c:a copy -c:v hevc_videotoolbox -q:v 65 -tag:v hvc1"
            opts_filters    = "-filter:v fps=30"
            opts_filters   += ",transpose=1" if original_path_no_ext.include?('iPad')

            command  = "ffmpeg -y -hide_banner -loglevel error"
            command += " -i \"#{original_path}\" #{opts_cut} #{opts_encoding} #{opts_filters}"
            command += " \"#{path_cut}\""
            system(command)

            # remove duplicate frames
            # https://stackoverflow.com/questions/37088517/remove-sequentially-duplicate-frames-when-using-ffmpeg
            # path_cleaned = original_path_no_ext + "-cleaned.mov"
            # `ffmpeg -y -hide_banner -loglevel error -i "#{path_cut}" -vf mpdecimate,setpts="N/(30*TB)" "#{path_cleaned}"`
        end
    end
end


def main
    recorder = Recorder.new
    recorder.observe
    log "Starting fastlane"
    system("fastlane snapshot --only_testing SaneScannerUITests/SaneScannerUITests_Video/testDeviceWithVideo --clear_previous_screenshots false --launch_arguments \"--VIDEO_SNAPSHOTS\"")
    recorder.stop_everything = true
    recorder.cleanup_videos
    log "DONE!"
    exit 0
end

main
