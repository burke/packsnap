$:.push File.expand_path("../lib", __FILE__)
require 'packsnap/version'

Gem::Specification.new do |s|
  s.name = "packsnap"
  s.version = Packsnap::VERSION
  s.summary = "Compressed Serialization using MessagePack and Snappy."
  s.description = %q{MessagePack is a binary-based efficient object serialization library. Snappy is a speed-oriented compression library. Packsnap is the composition of these libraries}
  s.author = "Burke Libbey"
  s.email = "burke@libbey.me"
  s.homepage = "https://github.com/burke/packsnap"
  s.has_rdoc = false
  s.files = `git ls-files`.split("\n")
  s.test_files = `git ls-files -- {test,spec}/*`.split("\n")
  s.require_paths = ["lib"]
  s.extensions = ['ext/packsnap/extconf.rb']

  s.add_development_dependency 'bundler', ['>= 1.0.0']
  s.add_development_dependency 'rake', ['>= 0.8.7']
  s.add_development_dependency 'rspec', ['>= 2.10.0']
  s.add_development_dependency 'json', ['~> 1.7']
  s.add_development_dependency 'yard', ['~> 0.8']
end
