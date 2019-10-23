#!/usr/bin/env ruby

require 'fileutils'

# Paths
SRC_DIR = Dir.pwd + "/sane-backends"
LOG_PATH = Dir.pwd + "/sane-log.txt"

# Release tag
GIT_TAG = "1.0.28"

# Build flags
MAKE_JOBS = 4

# Deletes file if it exists
def deleteFile(path)
    File.delete(path) if File.exist?(path)
end

# Makes sure the folder exists and is empty
def createEmptyFolder(path)
    FileUtils.remove_dir(path) if Dir.exist?(path)
    FileUtils.mkdir_p(path)
end

# Removes line containing "string" in file at given path
def removeLineInFile(path, string)
    lines = File.readlines(path)
    lines = lines.select { |line| not line.include? string }
    File.open(path, "w") { |file| 
        lines.each { |line|
            file.write line
        }
    }
end

# Run system call and log to log file
def systemWithLog(env = {}, command)
    status = system(env, command + " >> \"#{LOG_PATH}\" 2>&1")
    if status != true
        puts "An error has been encountered, please take a look at the log file "
        puts "=> #{LOG_PATH} "
        exit 0
    end
end

# Clones the backends repo and checkout the proper tag, only if not already present
def downloadAndCheckout
    if Dir.exist?(SRC_DIR)
        puts "Already downloaded sane-projets/backends"
        puts ""
        return
    end

    puts "Cloning sane-projets/backends"
    systemWithLog("git clone https://gitlab.com/sane-project/backends.git \"#{SRC_DIR}\"")

    puts "Checking out tag: #{GIT_TAG}"
    systemWithLog("cd #{SRC_DIR}; git checkout \"tags/#{GIT_TAG}\"")

    puts ""
end

# Builds a static library for the given architecture etc
def buildLib(arch, isSim, host, minSDK, output, optFlags)
    Dir.chdir(SRC_DIR)

    puts "Using architecture #{arch}, minimum iOS version: #{minSDK}"
    puts "   => Configuring"

    sdk     = isSim ? "iphonesimulator" : "iphoneos"
    sdkPath = `xcrun -sdk #{sdk} --show-sdk-path`

    minSDK = isSim ? "ios-simulator-version-min=#{minSDK}" : "iphoneos-version-min=#{minSDK}"

    hostFlags = "-arch #{arch} -m#{minSDK} -isysroot #{sdkPath}"

    # compiling with USB support would require compiling libusb which itself needs iOS IOKit headers. Unfortunately
    # those are private and the app would require root privileges to use the USB devices
    configureOptions = "--disable-local-backends --without-usb --enable-static --disable-shared --disable-warnings --enable-pthread --enable-silent-rules"

    flags = {
        "CC"        => `xcrun -find -sdk #{sdk} cc`,
        "CPP"       => `xcrun -find -sdk #{sdk} cpp`,
        "CFLAGS"    => hostFlags + " " + optFlags,
        "CXXFLAGS"  => hostFlags + " " + optFlags,
        "LDFLAGS"   => hostFlags,
    }

    dateStart = Time.now
    systemWithLog(flags, "./configure --host=#{host} --prefix=\"#{output}\" #{configureOptions}")

    # remove references of byte_order.h
    removeLineInFile("include/byteorder.h", "byte_order.h")

    puts "   => Building"
    systemWithLog("make clean")
    systemWithLog("make V=0 -j#{MAKE_JOBS}")
    systemWithLog("make install")

    elapsed = (Time.now - dateStart).round
    puts "   => Time: #{elapsed} seconds"
    puts ""
end

def mergeLibs(output, lib, archs)
    puts "Merging different architectures into #{output}/all/#{lib}"
    sourceFiles = archs.map { |arch| "#{output}/#{arch}/lib/#{lib}" }

    command = "lipo -create " + sourceFiles.join(" ") + " -output \"#{output}/all/#{lib}\""
    systemWithLog(command)
end

def buildAndMerge(output, optFlags) 
    # Build for real devices then simulators
    archs = {
        "armv7"  => { :isSim => false, :host => "arm-apple-darwin" },
        "armv7s" => { :isSim => false, :host => "arm-apple-darwin" },
        "arm64"  => { :isSim => false, :host => "armv64-apple-darwin" },
        "arm64e" => { :isSim => false, :host => "armv64-apple-darwin" },
        "i386"   => { :isSim => true,  :host => "i386-apple-darwin" },
        "x86_64" => { :isSim => true,  :host => "x86_64-apple-darwin" },
    }

    archs.each { |arch, opts| 
        buildLib(arch, opts[:isSim], opts[:host], 8.0, "#{output}/#{arch}", optFlags)
    }

    ## Merge libs
    createEmptyFolder(output + "/all")
    createEmptyFolder(output + "/all/sane")
    
    mergeLibs(output, "libsane.a",          archs.keys)
    mergeLibs(output, "sane/libsane-dll.a", archs.keys)
    mergeLibs(output, "sane/libsane-net.a", archs.keys)

    # Copy header files
    FileUtils.cp_r("#{output}/armv7/include/sane/sane.h",     "#{output}/all")
    FileUtils.cp_r("#{output}/armv7/include/sane/saneopts.h", "#{output}/all")

    # Copy translations
    puts "Copying string files"
    translationsPath = output + "/all/translations"
    createEmptyFolder(translationsPath)
    
    Dir.glob("#{SRC_DIR}/po/*.po").each { |file| 
        languageCode = File.basename(file, ".*").gsub("_", "-")
        destinationPath = translationsPath + "/" + languageCode + ".lproj"
        createEmptyFolder(destinationPath)
        FileUtils.cp(file, destinationPath + "/sane_strings.po")
    } 
end

def main
    ## Main script
    puts "Welcome to sane-backends build script to generate iOS libs, hope all goes well for you!"
    puts "This tools needs the developer command line tools to be installed."
    puts ""

    deleteFile(LOG_PATH)

    # Dowloads backend if not present
    downloadAndCheckout

    # Build
    buildAndMerge(Dir.pwd + "/sane-libs", "-O0 -g3 -U NDEBUG -D DEBUG=1")

    # Done!
    puts "All done!"
    system("tput bel")
end

main
