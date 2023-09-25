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
            .filter { |d| pid_in_simulator(d["udid"], "Backlit") != nil }
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

    VIDEOS_SIZES = {
        "iPhone 14 Pro Max"                     => [ 886, 1920],
        "iPhone 11 Pro Max"                     => [ 886, 1920],
        "iPhone 14 Pro"                         => [ 886, 1920],
        "iPhone X"                              => [ 886, 1920],
        "iPhone 8 Plus"                         => [1080, 1920],
        "iPhone 8"                              => [ 750, 1334],

        "iPhone SE (1st generation)"            => [1080, 1920],
        "iPad Pro (12.9-inch) (6th generation)" => [1600, 1200],
        "iPad Pro (11-inch) (4th generation)"   => [1600, 1200],
        "iPad Pro (12.9-inch) (2nd generation)" => [1600, 1200],
        "iPad Pro (10.5-inch)"                  => [1600, 1200],
        "iPad Pro (9.7-inch)"                   => [1200,  900],
    }
    def resize_videos
        log "Resizing videos"

        videos = Dir.glob("#{Dir.pwd}/fastlane/recordings/**/*.mov")
        videos.each do |original_path|
            filename = Pathname(original_path).sub_ext('').basename.to_s
            size = VIDEOS_SIZES[filename]
            next log("Ignoring #{filename}...") unless size

            log "Resizing #{filename} to #{size[0]}x#{size[1]}"

            path_resized = Pathname(original_path).sub_ext('').to_s + "-resized.mov"

            opts_size       = "-vf \"scale=#{size[0]}:#{size[1]}, setpts=0.45*PTS, fps=30\""
            opts_encoding   = "-c:a aac -c:v hevc_videotoolbox -q:v 65 -tag:v hvc1 -shortest"

            command  = "ffmpeg -y -hide_banner -loglevel error"
            command += " -f lavfi -i anullsrc=channel_layout=stereo:sample_rate=44100"
            command += " -i \"#{original_path}\" #{opts_size} #{opts_encoding}"
            command += " \"#{path_resized}\""
            system(command)

        end
    end

    TRIM_BEFORE = 1.to_f
    TRIM_AFTER = 3.to_f
    def cleanup_videos
        log "Cleaning up videos"

        videos = Dir.glob("#{Dir.pwd}/fastlane/recordings/**/*.mov")
        videos.each do |original_path|
            filename = Pathname(original_path).sub_ext('').basename.to_s
            next unless filename.end_with?('-resized')

            puts "Cleaning up #{original_path.gsub(Dir.pwd + '/fastlane/recordings/', '')}"

            # get duration
            duration = `ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "#{original_path}"`.to_f

            # trim
            path_cut = Pathname(original_path).sub_ext('').to_s + "-cut.mov"
            t_start = TRIM_BEFORE
            t_end = duration - TRIM_AFTER 

            opts_cut        = "-ss #{t_start} -to #{t_end}"
            opts_encoding   = "-c:a copy -c:v hevc_videotoolbox -q:v 65 -tag:v hvc1"

            command  = "ffmpeg -y -hide_banner -loglevel error"
            command += " -i \"#{original_path}\" #{opts_cut} #{opts_encoding}"
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
    system("fastlane snapshot --only_testing BacklitUITests/BacklitUITests_Video/testDeviceWithVideo --clear_previous_screenshots false --launch_arguments \"--VIDEO_SNAPSHOTS\"")
    recorder.stop_everything = true
    recorder.resize_videos
    #recorder.cleanup_videos
    log "DONE!"
    exit 0
end

main
