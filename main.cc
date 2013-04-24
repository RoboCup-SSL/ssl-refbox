#include "configuration.h"
#include "gamecontroller.h"
#include "legacypublisher.h"
#include "logger.h"
#include "mainwindow.h"
#include "protobufpublisher.h"
#include "publisher.h"
#include <exception>
#include <iostream>
#include <locale>
#include <memory>
#include <string>
#include <vector>
#include <glibmm/convert.h>
#include <glibmm/exception.h>
#include <glibmm/optioncontext.h>
#include <glibmm/optionentry.h>
#include <glibmm/optiongroup.h>
#include <google/protobuf/stubs/common.h>
#include <gtkmm/main.h>

namespace {
	int main_impl(int argc, char **argv) {
		// Set the current locale.
		std::locale::global(std::locale(""));

		// Parse the command-line arguments.
		Glib::OptionContext option_context;
		option_context.set_summary(u8"Runs the RoboCup Small Size League Referee Box.");
		option_context.set_description(u8"The Referee Box is © RoboCup Federation, 2003–2013.");

		Glib::OptionGroup option_group(u8"referee", u8"Referee Box Options", u8"Show Referee Box Options");

		Glib::OptionEntry config_file_entry;
		config_file_entry.set_long_name(u8"config");
		config_file_entry.set_short_name('C');
		config_file_entry.set_description(u8"Sets the name of the configuration file (default is “referee.conf”).");
		config_file_entry.set_arg_description(u8"CONFIGFILE");
		std::string config_filename = Glib::filename_from_utf8(u8"referee.conf");
		option_group.add_entry_filename(config_file_entry, config_filename);

		Glib::OptionEntry resume_entry;
		resume_entry.set_long_name(u8"resume");
		resume_entry.set_short_name('r');
		resume_entry.set_description(u8"Resumes an in-progress game by loading the saved state file (specified as SAVENAME in configuration).");
		bool resume = false;
		option_group.add_entry(resume_entry, resume);

		option_context.set_main_group(option_group);
		Gtk::Main kit(argc, argv, option_context);

		// Initialize the game objects.
		Configuration configuration(config_filename);

		// Start a logger.
		Logger logger(configuration.log_filename);
		configuration.dump(logger);

		// Construct the publishers.
		std::vector<Publisher *> publishers;
		std::unique_ptr<ProtobufPublisher> protobuf_publisher;
		if (!configuration.protobuf_port.empty()) {
			protobuf_publisher.reset(new ProtobufPublisher(configuration, logger));
			publishers.push_back(protobuf_publisher.get());
		}
		std::unique_ptr<LegacyPublisher> legacy_publisher;
		if (!configuration.legacy_port.empty()) {
			legacy_publisher.reset(new LegacyPublisher(configuration, logger));
			publishers.push_back(legacy_publisher.get());
		}

		// Construct the game controller that ties everything together.
		GameController controller(logger, configuration, publishers, resume);

		// Create and display a main window.
		MainWindow main_window(controller);
		kit.run(main_window);

        // Shut down protobuf.
        google::protobuf::ShutdownProtobufLibrary();

		return 0;
	}

	void print_exception(const Glib::Exception &exp) {
		std::cerr << "\nUnhandled exception:\n";
		std::cerr << "Type:   " << typeid(exp).name() << '\n';
		std::cerr << "Detail: " << exp.what() << '\n';
	}

	void print_exception(const std::exception &exp, bool first = true) {
		if (first) {
			std::cerr << "\nUnhandled exception:\n";
		} else {
			std::cerr << "Caused by:\n";
		}
		std::cerr << "Type:   " << typeid(exp).name() << '\n';
		std::cerr << "Detail: " << exp.what() << '\n';
		try {
			std::rethrow_if_nested(exp);
		} catch (const std::exception &exp) {
			print_exception(exp, false);
		}
	}
}

int main(int argc, char **argv) {
	try {
		return main_impl(argc, argv);
	} catch (const Glib::Exception &exp) {
		print_exception(exp);
	} catch (const std::exception &exp) {
		print_exception(exp);
	} catch (...) {
		std::cerr << "\nUnhandled exception:\n";
		std::cerr << "Type:   Unknown\n";
		std::cerr << "Detail: Unknown\n";
	}
	return 1;
}

