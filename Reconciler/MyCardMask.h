#pragma once

#include <poker_defs.h>

class MyCardMask
{
public:
	MyCardMask( ) 
	{ 
		CardMask_RESET( mask ); 
	}

	void Add( CardMask other )
	{
		CardMask_OR( mask, mask, other );
	}
	
	void Add( const std::string & str )
	{
		CardMask_SET( mask, ConvertStr( str ) );
	}

	CardMask Get( )
	{
		return mask;
	}

	int NumCards( )
	{
		return StdDeck.numCards( & mask );
	}

private:
	CardMask mask;

	int ConvertStr( const std::string & str )
	{
		int rank, suit;

		if( str.length( ) != 2 )
			goto fail;

		switch( str[ 0 ] )
		{
			case '2': rank = StdDeck_Rank_2; break;
			case '3': rank = StdDeck_Rank_3; break;
			case '4': rank = StdDeck_Rank_4; break;
			case '5': rank = StdDeck_Rank_5; break;
			case '6': rank = StdDeck_Rank_6; break;
			case '7': rank = StdDeck_Rank_7; break;
			case '8': rank = StdDeck_Rank_8; break;
			case '9': rank = StdDeck_Rank_9; break;
			case 'T': rank = StdDeck_Rank_TEN; break;
			case 'J': rank = StdDeck_Rank_JACK; break;
			case 'Q': rank = StdDeck_Rank_QUEEN; break;
			case 'K': rank = StdDeck_Rank_KING; break;
			case 'A': rank = StdDeck_Rank_ACE; break;
			default: goto fail;
		}

		switch( str[ 1 ] )
		{
			case 'h': suit = StdDeck_Suit_HEARTS; break;
			case 'd': suit = StdDeck_Suit_DIAMONDS; break;
			case 'c': suit = StdDeck_Suit_CLUBS; break;
			case 's': suit = StdDeck_Suit_SPADES; break;
			default: goto fail;
		}

		return StdDeck_MAKE_CARD( rank, suit );

fail: 
		throw Exception( "Card represented by illegal string: '" + str + "'." );
	}

};
