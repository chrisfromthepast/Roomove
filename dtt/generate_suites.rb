#!/usr/bin/env ruby
require "open3"
require "optparse"
require "pathname"

options = {
  suite_generator: "SuiteGenerator.rb",
  output_dir: "dtt/generated"
}

OptionParser.new do |parser|
  parser.banner = "Usage: ruby dtt/generate_suites.rb [options]"
  parser.on("--suite-generator PATH", "Path to SuiteGenerator.rb") { |value| options[:suite_generator] = value }
  parser.on("--output-dir PATH", "Directory for generated suites") { |value| options[:output_dir] = value }
end.parse!

root = Pathname.new(__dir__).parent
cycle_suite = root.join("dtt", "suites", "DSH_TI_CycleCounts.yml")
cancel_suite = root.join("dtt", "suites", "DSH_SigCancellation.yml")
output_dir = root.join(options[:output_dir])
suite_generator = options[:suite_generator]

begin
  Dir.mkdir(output_dir) unless output_dir.exist?
rescue SystemCallError => error
  warn("ERROR: unable to create output directory '#{output_dir}': #{error}")
  exit(2)
end

def run_generator(suite_generator, yaml_path, output_dir)
  command = [
    "ruby",
    suite_generator,
    "--input",
    yaml_path.to_s,
    "--output",
    output_dir.to_s
  ]
  stdout, stderr, status = Open3.capture3(*command)
  puts(stdout) unless stdout.empty?
  warn(stderr) unless stderr.empty?
  unless status.success?
    warn("ERROR: SuiteGenerator failed for #{yaml_path}")
    return false
  end
  true
rescue StandardError => error
  warn("ERROR: exception while generating suite from #{yaml_path}: #{error}")
  false
end

cycle_ok = run_generator(suite_generator, cycle_suite, output_dir)
cancel_ok = run_generator(suite_generator, cancel_suite, output_dir)

unless cycle_ok && cancel_ok
  exit(1)
end

puts("PASS: generated DTT suites in #{output_dir}")
