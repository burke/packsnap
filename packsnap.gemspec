$:.push File.expand_path("../lib", __FILE__)
require 'packsnap/version'

Gem::Specification.new do |s|
  s.name = "packsnap"
  s.version = Packsnap::VERSION
  s.summary = "MessagePack, a binary-based efficient data interchange format."
  s.description = %q{MessagePack is a binary-based efficient object serialization library. It enables to exchange structured objects between many languages like JSON. But unlike JSON, it is very fast and small.}
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
