#include <optional>
#include <string>
#include <libchess/Position.h>


std::optional<libchess::Move> SAN_to_move(std::string san_move, const libchess::Position & pos)
{
	libchess::PieceType type       = libchess::constants::PAWN;
	std::optional<libchess::PieceType> promote_to;
	int                 base_line  = pos.side_to_move() == libchess::constants::WHITE ? 0 : 7;
	std::string         from;
	std::string         to;
	size_t              x          = std::string::npos;
	int                 fx         = -1;
	int                 fy         = -1;
	int                 tx         = -1;
	int                 ty         = -1;

	/* '+' at the end means check */
	size_t markers_index = san_move.find('+');
	if (markers_index != std::string::npos)
		san_move = san_move.substr(0, markers_index);

	/* '#' at the end means check-mate */
	markers_index = san_move.find('#');
	if (markers_index != std::string::npos)
		san_move = san_move.substr(0, markers_index);

	/* o-o/o-o-o (castling) */
	if (san_move == "o-o") {
		// FIXME: check if castling is allowed
		return libchess::Move(libchess::Square::from(libchess::File(4), libchess::Rank(base_line)).value(),
				libchess::Square::from(libchess::File(6), libchess::Rank(base_line)).value(),
				libchess::Move::Type::CASTLING);
	}
	if (san_move == "o-o-o") {
		// FIXME: check if castling is allowed
		return libchess::Move(libchess::Square::from(libchess::File(4), libchess::Rank(base_line)).value(),
				libchess::Square::from(libchess::File(2), libchess::Rank(base_line)).value(),
				libchess::Move::Type::CASTLING);
	}

	/* the object-type is in the first character of the string,
	 * it can be recognized by that it is an uppercase character.
	 * if it is not there (the first char is lowercase), it is
	 * a pawn
	 */
	if (isupper(san_move.at(0))) {
		type     = libchess::PieceType::from(san_move.at(0)).value();
		san_move = san_move.substr(1);
	}

	/* if the object type is a pawn, there might be a '=Q/B/N/R'
	 * behind the string indicating the object it promotes to
	 */
	if (type == libchess::constants::PAWN) {
		size_t is = san_move.find('=');
		if (is != std::string::npos) {
			promote_to = libchess::PieceType::from(san_move.at(is + 1)).value();
			san_move   = san_move.substr(0, is);
		}
		/*else {
			size_t last_char_index = san_move.length() - 1;
			char   last_character  = san_move.at(last_char_index);

			if (isupper(last_character)) {
				promote_to = libchess::PieceType::from(last_character).value();
				san_move   = san_move.substr(0, last_char_index);
			}
		}
		*/

		if (san_move.empty())
			return { };
	}

	/* check for capture move & split move into from/to string */
	x = san_move.find('x');
	if (x == std::string::npos)
		x = san_move.find('X');

	if (x != std::string::npos) {
		from = san_move.substr(0, x);
		to   = san_move.substr(x + 1);
	}
	else {
		size_t move_str_len = san_move.length();

		if (move_str_len > 2) {
			from = san_move.substr(0, move_str_len - 2);
			to   = san_move.substr(move_str_len - 2);
		}
		else {
			from.clear();
			to = san_move;
		}
	}

	if (to.size() < 2)
		return { };

	/* convert to-string to a position */
	tx = to.at(0) - 'a';
	ty = to.at(1) - '1';

	/* convert from-string, if available */
	if (from.empty()) {
		fx = fy = -1;
	}
	else if (from.length() == 2) {
		fx = from.at(0) - 'a';
		fy = from.at(1) - '1';
	}
	else if (from.length() == 1) {
		char fromChar = from.at(0);

		if (isdigit(fromChar)) {
			fx = -1;
			fy = fromChar - '1';
		}
		else {
			fx = fromChar - 'a';
			fy = -1;
		}
	}

	/* now find the move in the movelist */
	for(auto & current_move: pos.legal_move_list()) {
		if (current_move.to_square().file() != tx || current_move.to_square().rank() != ty)
			continue;

		if (fx != -1 && current_move.from_square().file() != fx)
			continue;

		if (fy != -1 && current_move.from_square().rank() != fy)
			continue;

		// determing the type is tricky
		if (pos.piece_on(current_move.from_square()).value().type() != type)
			continue;

		if (promote_to.has_value() != current_move.promotion_piece_type().has_value() ||
		    (promote_to.has_value() && promote_to.value() != current_move.promotion_piece_type().value())) {
			continue;
		}

		return current_move;
	}

	return { };
}

std::optional<libchess::Move> validate_move(const libchess::Move & move, const libchess::Position & p)
{
	/* now find the move in the movelist */
	for(auto & current_move: p.legal_move_list()) {
		if (current_move.to_square().file() != move.to_square().file() || current_move.to_square().rank() != move.to_square().rank())
			continue;

		if (move.from_square().file() != -1 && current_move.from_square().file() != move.from_square().file())
			continue;

		if (move.from_square().rank() != -1 && current_move.from_square().rank() != move.from_square().rank())
			continue;

		if (move.promotion_piece_type().has_value() != current_move.promotion_piece_type().has_value() ||
			(move.promotion_piece_type().has_value() && move.promotion_piece_type().value() != current_move.promotion_piece_type().value())) {
			continue;
		}

		return current_move;
	}

	return { };
}
