require 'packsnap/version'

here = File.expand_path(File.dirname(__FILE__))

require 'rbconfig'
prebuilt = File.join(here, RUBY_PLATFORM, RbConfig::CONFIG['ruby_version'])
if File.directory?(prebuilt)
  require File.join(prebuilt, "packsnap")
else
  require File.join(here, '..', 'ext', 'packsnap', 'packsnap')
end

