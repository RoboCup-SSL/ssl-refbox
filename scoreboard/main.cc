#include "gamestate.h"
#include "imagedb.h"
#include "mainwindow.h"
#include "socket.h"
#include <exception>
#include <iostream>
#include <locale>
#include <glibmm/convert.h>
#include <glibmm/exception.h>
#include <glibmm/keyfile.h>
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
		option_context.set_summary(u8"Runs the RoboCup Small Size League Scoreboard.");
		option_context.set_summary(u8"The Referee Box is © RoboCup Federation, 2013–2013.");

		Glib::OptionGroup option_group(u8"scoreboard", u8"Scoreboard Options", u8"Show Scoreboard Options");

		Glib::OptionEntry mc_interface_entry;
		mc_interface_entry.set_long_name(u8"interface");
		mc_interface_entry.set_short_name('i');
		mc_interface_entry.set_description(u8"Sets the network interface name on which packets will be received.");
		mc_interface_entry.set_arg_description(u8"INTERFACE");
		Glib::ustring mc_interface(u8"eth0");
		option_group.add_entry(mc_interface_entry, mc_interface);

		Glib::OptionEntry mc_group_entry;
		mc_group_entry.set_long_name(u8"group");
		mc_group_entry.set_short_name('g');
		mc_group_entry.set_description(u8"Sets the multicast group to join.");
		mc_group_entry.set_arg_description(u8"ADDRESS");
		Glib::ustring mc_group(u8"224.5.23.1");
		option_group.add_entry(mc_group_entry, mc_group);

		Glib::OptionEntry mc_port_entry;
		mc_port_entry.set_long_name(u8"port");
		mc_port_entry.set_short_name('p');
		mc_port_entry.set_description(u8"Sets the UDP port on which packets will be received.");
		mc_port_entry.set_arg_description(u8"PORT");
		Glib::ustring mc_port(u8"10003");
		option_group.add_entry(mc_port_entry, mc_port);

		option_context.set_main_group(option_group);
		Gtk::Main kit(argc, argv, option_context);

		// Load configuration file.
		Glib::KeyFile kf;
		kf.load_from_file("scoreboard.conf");

		// Load the flags and logos.
		const image_database_t &flags = load_image_database("flags");
		const image_database_t &logos = load_image_database("logos");

		// Start receiving and updating game state.
		Socket::init_system();
		GameState state(mc_interface, mc_group, mc_port);

		// Create and display a main window.
		MainWindow main_window(state, flags, logos, kf);
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

