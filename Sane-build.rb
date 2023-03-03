#!/usr/bin/env ruby

require 'fileutils'
require 'shellwords'
require 'pathname'

# Paths
GIT_DIR = Pathname.pwd + 'Sane-source'
DEST_DIR = Pathname.pwd + 'Sane'
FINAL_DIR = DEST_DIR + 'all'
LOG_PATH = Pathname.pwd + 'Sane-build-log.txt'

# Release tag
GIT_TAG = '1.2.1'.freeze

# Build flags
MAKE_JOBS = 4
FLAGS = '-O0 -g3 -U NDEBUG -D DEBUG=1'.freeze

# Builds
BUILDS = {
    # iOS
    'ios-armv7'  => { platform: 'ios', arch: 'armv7',  sdk: 'iphoneos', min: 'iphoneos-version-min=9.0', host: 'arm-apple-darwin' },
    'ios-armv7s' => { platform: 'ios', arch: 'armv7s', sdk: 'iphoneos', min: 'iphoneos-version-min=9.0', host: 'arm-apple-darwin' },
    'ios-arm64'  => { platform: 'ios', arch: 'arm64',  sdk: 'iphoneos', min: 'iphoneos-version-min=9.0', host: 'armv64-apple-darwin' },
    'ios-arm64e' => { platform: 'ios', arch: 'arm64e', sdk: 'iphoneos', min: 'iphoneos-version-min=9.0', host: 'armv64-apple-darwin' },

    # iOS Simulator
    'sim-i386'   => { platform: 'sim', arch: 'i386',   sdk: 'iphonesimulator', min: 'ios-simulator-version-min=9.0', host: 'i386-apple-darwin',   target: 'i386-apple-ios9.0-simulator' },
    'sim-x86_64' => { platform: 'sim', arch: 'x86_64', sdk: 'iphonesimulator', min: 'ios-simulator-version-min=9.0', host: 'x86_64-apple-darwin', target: 'x86_64-apple-ios9.0-simulator' },
    'sim-arm64'  => { platform: 'sim', arch: 'arm64',  sdk: 'iphonesimulator', min: 'ios-simulator-version-min=9.0', host: 'arm64-apple-darwin',  target: 'arm64-apple-ios9.0-simulator' },

    # Catalyst
    "catalyst-x86_64" => { platform: 'catalyst', arch: 'x86_64', sdk: 'macosx', min: 'iphoneos-version-min=13.1', host: 'x86_64-apple-darwin', target: 'x86_64-apple-ios13.1-macabi' },
    "catalyst-arm64"  => { platform: 'catalyst', arch: 'arm64',  sdk: 'macosx', min: 'iphoneos-version-min=13.1', host: 'arm64-apple-darwin',  target: 'arm64-apple-ios13.1-macabi' },

    # macOS
    "macos-x86-64" => { platform: 'macosx', arch: 'x86_64', sdk: 'macosx', min: 'macosx-version-min=11.0', host: 'x86_64-apple-darwin', target: 'x86_64-apple-darwin' },
    "macos-arm64"  => { platform: 'macosx', arch: 'arm64',  sdk: 'macosx', min: 'macosx-version-min=11.0', host: 'arm64-apple-darwin',  target: 'arm64-apple-darwin' },
}.freeze

# Makes sure the folder exists and is empty
def create_empty_folder(path)
    FileUtils.remove_dir(path) if Dir.exist?(path)
    path.mkpath
end

# Run system call and log to log file
def system_with_log(command, env = {})
    LOG_PATH.open('a+') do |f|
        f.puts('')
        f.puts('###################################')
        f.puts("## #{command}")
        f.puts('')
    end

    status = system(env, command + " >> \"#{LOG_PATH}\" 2>&1")
    if status != true
        puts ''
        puts 'An error has been encountered, please take a look at the log file '
        puts "=> #{LOG_PATH} "
        puts ''
        puts 'Here is an excerpt:'
        puts IO.readlines(LOG_PATH).last(10)
        exit 0
    end
end

# Clones the backends repo and checkout the proper tag, only if not already present
def checkout
    if GIT_DIR.exist?
        puts 'Already downloaded sane-projets/backends'
        puts ''
        return
    end

    puts 'Cloning sane-projets/backends'
    system_with_log("git clone https://gitlab.com/sane-project/backends.git \"#{GIT_DIR}\"")

    puts "Checking out tag: #{GIT_TAG}"
    system_with_log("cd \"#{GIT_DIR}\"; git checkout \"tags/#{GIT_TAG}\"")

    puts "Directory: #{Dir.pwd}"
    puts 'Running autogen.sh'
    system_with_log("cd \"#{GIT_DIR}\"; ./autogen.sh")

    puts ''
end

# Builds a static library for the given target and architecture
# some notes on catalyst support: https://github.com/CocoaPods/CocoaPods/issues/8877
def build_lib(name)
    options = BUILDS[name]
    start_date = Time.now
    output_dir = DEST_DIR + name

    puts "Building target #{name}, minimum SDK version: #{options[:min]}"

    if output_dir.exist?
        puts '   => Skipping'
        puts ''
        return
    end

    sdk_path = `xcrun -sdk #{options[:sdk]} --show-sdk-path`.chomp
    host_flags = [
        "-arch #{options[:arch]}",
        "-m#{options[:min]}",
        "-isysroot #{sdk_path}",
    ]
    host_flags << "-target #{options[:target]}" if options.key?(:target)
    host_flags = host_flags.join(' ')

    flags = {
        "CC"        => `xcrun -find -sdk #{options[:sdk]} clang`,
        "CXX"       => `xcrun -find -sdk #{options[:sdk]} clang++`,
        "CPP"       => `xcrun -find -sdk #{options[:sdk]} cpp`,
        "CFLAGS"    => host_flags + " " + FLAGS,
        "CXXFLAGS"  => host_flags + " " + FLAGS + " ",
        "LDFLAGS"   => host_flags + " -v",
    }

    # compiling with USB support would require compiling libusb which itself needs iOS IOKit headers. Unfortunately
    # those are private and the app would require root privileges to use the USB devices
    configure_options = '--disable-local-backends --without-libcurl --without-snmp --without-usb --without-poppler-glib --enable-static --disable-shared --disable-warnings --enable-pthread --enable-silent-rules'

    Dir.chdir(GIT_DIR)
    puts '   => Configuring'
    system_with_log("./configure --host=#{options[:host]} --prefix=#{output_dir} #{configure_options}", flags)

    puts '   => Building'
    system_with_log('make clean')
    system_with_log("make V=0 -j#{MAKE_JOBS}")
    system_with_log('make install')

    elapsed = (Time.now - start_date).round
    puts "   => Time: #{elapsed} seconds"
    puts ''
end

def merge_libs(platform)
    builds = BUILDS.select { |_, opts| opts[:platform] == platform }

    input_libs = ['libsane.a', 'sane/libsane-dll.a', 'sane/libsane-net.a']
    output_combined_lib = 'sane.a'
    output_fat_lib = DEST_DIR + "#{platform}-all" + "libSane.a"
    puts "Merging libs into #{output_combined_lib} for platform #{platform} then all architectures into #{output_fat_lib.relative_path_from(DEST_DIR)}"

    # This will merge libsane.a, libsane-dll.a and libsane-net.a into a single sane.a
    combined = builds.keys.map do |name|
        input = input_libs.map { |lib| DEST_DIR + name + "lib" + lib }.filter { |lib| File.exist?(lib) }
        output = DEST_DIR + name + "lib" + output_combined_lib

        command = "libtool -static #{input.join(' ')} -o #{output}"
        system_with_log(command)
        output
    end

    # then it will create a single libSane.a containing all architectures
    create_empty_folder(output_fat_lib.dirname)

    command = 'lipo -create ' + combined.join(' ') + " -output \"#{output_fat_lib}\""
    system_with_log(command)

    output_fat_lib
end

def copy_headers
    input_dir = DEST_DIR + BUILDS.keys.first + 'include' + 'sane'
    input_files = [input_dir + 'sane.h', input_dir + 'saneopts.h']

    output_dir = DEST_DIR + 'headers'
    create_empty_folder(output_dir)

    input_files.each do |input_file|
        FileUtils.cp_r(input_file, output_dir)
    end

    output_dir
end

def create_framework(libs, headers)
    output_file = FINAL_DIR + 'Sane.xcframework'

    puts 'Creating XCFramework'
    create_empty_folder(output_file)

    options = libs
        .map { |lib| "-library #{lib} -headers #{headers}" }
        .join(' ')

    command = "xcodebuild -create-xcframework #{options} -output #{output_file}"
    system_with_log(command)

    output_file
end

def add_module_map_to_framework(framework_path)
    contents = <<-eos
module Sane {
    header "sane.h"
    header "saneopts.h"
    export *
}
    eos

    outputs = Dir.glob("#{framework_path}/*/Headers")
    outputs.each do |path|
        File.open(path + '/module.modulemap', 'w') do |f|
            f << contents
        end
    end
end

def add_license_to_framework(framework_path)
    output_file = framework_path + 'LICENSE.txt'
    full_license_path = GIT_DIR + 'LICENSE'
    headers_licenses = [
        GIT_DIR + 'backend' + 'net.c',
        GIT_DIR + 'backend' + 'dll.c'
    ]

    puts "Adding #{output_file.basename} to XCFramework"
    FileUtils.cp_r(full_license_path, output_file, remove_destination: true)

    headers_licenses.each do |header|
        content = header.open.read.split('*/').first + '*/'
        output_file.open('a') do |f|
            f << "\n\n"
            f << "/***** #{header.relative_path_from(GIT_DIR)} *****/"
            f << "\n"
            f << content
        end
    end

    output_file
end

def copy_translations
    puts 'Copying string files'

    output_dir = FINAL_DIR + 'translations'
    create_empty_folder(output_dir)

    Dir.glob("#{GIT_DIR}/po/*.po").each do |file|
        next if file.include?('@')

        locale = File.basename(file, '.*').gsub('_', '-')
        locale_output_dir = output_dir + "#{locale}.lproj"
        create_empty_folder(locale_output_dir)
        FileUtils.cp(file, locale_output_dir + 'sane_strings.po')
    end
end

def main
    ## Main script
    puts 'Welcome to sane-backends build script to generate iOS libs, hope all goes well for you!'
    puts 'This tools needs the developer command line tools to be installed.'
    puts ''
    puts 'You may also need the following tools:'
    puts '    brew install automake autoconf autoconf-archive libtool gettext'
    puts ''
    puts 'Then add gettext to your $PATH, using /usr/local/opt/gettext/bin or /opt/homebrew/bin/'
    puts ''

    LOG_PATH.delete if LOG_PATH.exist?

    # Downloads backend if not present
    checkout

    # Build static libs
    BUILDS.keys.each { |name| build_lib(name) }

    # Merge libs
    platforms = BUILDS.values.map { |o| o[:platform] }.uniq
    libs = platforms.map do |platform|
        merge_libs(platform)
    end

    # Copy header files
    headers_path = copy_headers

    # Create XCFramework
    framework_path = create_framework(libs, headers_path)
    add_module_map_to_framework(framework_path)

    # Add license
    add_license_to_framework(framework_path)

    # Copy translations
    copy_translations

    # Done!
    puts 'All done!'
    system('tput bel')
end

main
