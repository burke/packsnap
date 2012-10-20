
require 'bundler'
Bundler::GemHelper.install_tasks

require 'rspec/core'
require 'rspec/core/rake_task'
require 'yard'

RSpec::Core::RakeTask.new(:spec) do |t|
  t.rspec_opts = ["-c", "-f progress"]
  t.rspec_opts << "-Ilib"
  t.pattern = 'spec/**/*_spec.rb'
  t.verbose = true
end

task :spec => :compile

desc 'Run RSpec code examples and measure coverage'
task :coverage do |t|
  ENV['SIMPLE_COV'] = '1'
  Rake::Task["spec"].invoke
end

desc 'Generate YARD document'
YARD::Rake::YardocTask.new(:doc) do |t|
  t.files   = ['lib/packsnap/version.rb','doclib/**/*.rb']
  t.options = []
  t.options << '--debug' << '--verbose' if $trace
end

def create_gemspec(platform, extra_globs, remove_globs)
  spec = eval(File.read('packsnap.gemspec'))

  extra_globs.each {|glob| spec.files += Dir[glob] }
  remove_globs.each {|glob| spec.files -= Dir[glob] }

  if platform && !platform.empty?
    spec.platform = platform
  end

  spec
end

def create_gem(platform, extra_globs, remove_globs)
  spec = create_gemspec(platform, extra_globs, remove_globs)
  name = Gem::Builder.new(spec).build

  FileUtils.mkdir_p('pkg/')
  FileUtils.mv(name, 'pkg/')
end

def run_command(cmd, env = {})
  puts env.map {|k,v| "#{k}=\"#{v}\"" }.join(' ')
  puts cmd
  if env.empty?
    system(cmd)  # for jruby
  else
    system(env, cmd)
  end

  if $?.to_i != 0
    raise "command failed (#{$?.to_i}): #{cmd}"
  end
end

task :default => :gem

desc "Create gem package"
task "gem" do
  #if RUBY_PLATFORM =~ /java/
  #  Rake::Task["gem:java"].invoke
  #else
    create_gem(nil, ["lib/packsnap/**/*.{so,bundle}"], ["ext/**/*"])
  #end
end

desc "Create precompiled gem package"
task "gem:build" do
  config = YAML.load_file("packsnap.build.yml")

  platform = config['platform']
  archs = []

  begin
    FileUtils.rm_rf Dir["lib/packsnap/*-*"]

    config['versions'].each {|ver|
      ruby = ver.delete('ruby')
      env = ver.delete('env') || {}
      arch = `'#{ruby}' -rrbconfig -e "puts RUBY_PLATFORM"`.strip
      ruby_version = `'#{ruby}' -rrbconfig -e "puts RbConfig::CONFIG['ruby_version']"`.strip
      vpath = "#{arch}/#{ruby_version}"

      run_command "cd ext/packsnap && '#{ruby}' extconf.rb && make clean && make", env

      path = Dir['ext/packsnap/packsnap.{so,bundle}'].first
      FileUtils.mkdir_p("lib/packsnap/#{vpath}")
      FileUtils.cp(path, "lib/packsnap/#{vpath}/")

      archs << arch
    }

    create_gem(platform, ["lib/packsnap/**/*.{so,bundle}"], ["ext/**/*"])

  ensure
    FileUtils.rm_rf Dir["lib/packsnap/*-*"]
  end
end

#task "gem:java" do
#  Rake::Task["compile:java"].invoke
#
#  begin
#    FileUtils.mkdir_p 'lib/packsnap/java'
#    FileUtils.cp Dir["ext/java/*.jar"], "lib/"
#    FileUtils.cp "ext/java/packsnap.jar", "lib/packsnap"
#
#    create_gem('java', [], ["ext/packsnap/**/*"])
#  ensure
#    FileUtils.rm_rf "ext/java/build"
#    FileUtils.rm_rf "lib/packsnap/java"
#    FileUtils.rm_rf Dir["lib/*.jar"]
#  end
#end

task "compile" do
  #if RUBY_PLATFORM =~ /java/
  #  Rake::Task["compile:java"].invoke
  #else
    ruby = 'ruby'  # TODO use self
    run_command "cd ext/packsnap && '#{ruby}' extconf.rb && make clean && make"
  #end
end

#task "compile:java" do
#  Dir.chdir('ext/java')
#  begin
#    jruby_home = RbConfig::CONFIG['prefix']
#    classpath = ["#{jruby_home}/lib/jruby.jar"] + Dir['*.jar']
#    files = Dir['packsnap/**/*.java']
#
#    FileUtils.rm_rf "ext/java/build"
#    FileUtils.mkdir_p 'build'
#    run_command "javac -cp '#{classpath.join(':')}' -d build #{files.join(' ')}"
#    run_command "jar cvf packsnap.jar -C build/ ."
#  ensure
#    Dir.chdir('../..')
#  end
#end

