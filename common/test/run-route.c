#include "config.h"
#include <assert.h>
#include <common/channel_type.h>
#include <common/dijkstra.h>
#include <common/gossmap.h>
#include <common/gossip_store.h>
#include <common/route.h>
#include <common/sciddir_or_pubkey.h>
#include <common/setup.h>
#include <common/utils.h>
#include <bitcoin/chainparams.h>
#include <stdio.h>
#include <wire/peer_wiregen.h>
#include <unistd.h>

/* AUTOGENERATED MOCKS START */
/* Generated stub for fromwire_bigsize */
bigsize_t fromwire_bigsize(const u8 **cursor UNNEEDED, size_t *max UNNEEDED)
{ fprintf(stderr, "fromwire_bigsize called!\n"); abort(); }
/* Generated stub for fromwire_channel_id */
bool fromwire_channel_id(const u8 **cursor UNNEEDED, size_t *max UNNEEDED,
			 struct channel_id *channel_id UNNEEDED)
{ fprintf(stderr, "fromwire_channel_id called!\n"); abort(); }
/* Generated stub for fromwire_tlv */
bool fromwire_tlv(const u8 **cursor UNNEEDED, size_t *max UNNEEDED,
		  const struct tlv_record_type *types UNNEEDED, size_t num_types UNNEEDED,
		  void *record UNNEEDED, struct tlv_field **fields UNNEEDED,
		  const u64 *extra_types UNNEEDED, size_t *err_off UNNEEDED, u64 *err_type UNNEEDED)
{ fprintf(stderr, "fromwire_tlv called!\n"); abort(); }
/* Generated stub for sciddir_or_pubkey_from_node_id */
bool sciddir_or_pubkey_from_node_id(struct sciddir_or_pubkey *sciddpk UNNEEDED,
				    const struct node_id *node_id UNNEEDED)
{ fprintf(stderr, "sciddir_or_pubkey_from_node_id called!\n"); abort(); }
/* Generated stub for towire_bigsize */
void towire_bigsize(u8 **pptr UNNEEDED, const bigsize_t val UNNEEDED)
{ fprintf(stderr, "towire_bigsize called!\n"); abort(); }
/* Generated stub for towire_channel_id */
void towire_channel_id(u8 **pptr UNNEEDED, const struct channel_id *channel_id UNNEEDED)
{ fprintf(stderr, "towire_channel_id called!\n"); abort(); }
/* Generated stub for towire_tlv */
void towire_tlv(u8 **pptr UNNEEDED,
		const struct tlv_record_type *types UNNEEDED, size_t num_types UNNEEDED,
		const void *record UNNEEDED)
{ fprintf(stderr, "towire_tlv called!\n"); abort(); }
/* AUTOGENERATED MOCKS END */

static void write_to_store(int store_fd, const u8 *msg)
{
	struct gossip_hdr hdr;

	hdr.flags = cpu_to_be16(0);
	hdr.len = cpu_to_be16(tal_count(msg));
	/* We don't actually check these! */
	hdr.crc = 0;
	hdr.timestamp = 0;
	assert(write(store_fd, &hdr, sizeof(hdr)) == sizeof(hdr));
	assert(write(store_fd, msg, tal_count(msg)) == tal_count(msg));
}

static void update_connection(int store_fd,
			      const struct node_id *from,
			      const struct node_id *to,
			      u32 base_fee, s32 proportional_fee,
			      u32 delay,
			      bool disable)
{
	struct short_channel_id scid;
	secp256k1_ecdsa_signature dummy_sig;
	u8 *msg;

	/* So valgrind doesn't complain */
	memset(&dummy_sig, 0, sizeof(dummy_sig));

	/* Make a unique scid. */
	memcpy(&scid, from, sizeof(scid) / 2);
	memcpy((char *)&scid + sizeof(scid) / 2, to, sizeof(scid) / 2);

	msg = towire_channel_update(tmpctx,
				    &dummy_sig,
				    &chainparams->genesis_blockhash,
				    scid, 0,
				    ROUTING_OPT_HTLC_MAX_MSAT,
				    node_id_idx(from, to)
				    + (disable ? ROUTING_FLAGS_DISABLED : 0),
				    delay,
				    AMOUNT_MSAT(0),
				    base_fee,
				    proportional_fee,
				    AMOUNT_MSAT(100000 * 1000));

	write_to_store(store_fd, msg);
}

static void add_connection(int store_fd,
			   const struct node_id *from,
			   const struct node_id *to,
			   u32 base_fee, s32 proportional_fee,
			   u32 delay)
{
	struct short_channel_id scid;
	secp256k1_ecdsa_signature dummy_sig;
	struct secret not_a_secret;
	struct pubkey dummy_key;
	u8 *msg;
	const struct node_id *ids[2];

	/* So valgrind doesn't complain */
	memset(&dummy_sig, 0, sizeof(dummy_sig));
	memset(&not_a_secret, 1, sizeof(not_a_secret));
	pubkey_from_secret(&not_a_secret, &dummy_key);

	/* Make a unique scid. */
	memcpy(&scid, from, sizeof(scid) / 2);
	memcpy((char *)&scid + sizeof(scid) / 2, to, sizeof(scid) / 2);

	if (node_id_cmp(from, to) > 0) {
		ids[0] = to;
		ids[1] = from;
	} else {
		ids[0] = from;
		ids[1] = to;
	}
	msg = towire_channel_announcement(tmpctx, &dummy_sig, &dummy_sig,
					  &dummy_sig, &dummy_sig,
					  /* features */ NULL,
					  &chainparams->genesis_blockhash,
					  scid,
					  ids[0], ids[1],
					  &dummy_key, &dummy_key);
	write_to_store(store_fd, msg);

	update_connection(store_fd, from, to, base_fee, proportional_fee,
			  delay, false);
}

static bool channel_is_between(const struct gossmap *gossmap,
			       const struct route_hop *route,
			       const struct gossmap_node *a,
			       const struct gossmap_node *b)
{
	const struct gossmap_chan *c = gossmap_find_chan(gossmap, &route->scid);
	if (c->half[route->direction].nodeidx
	    != gossmap_node_idx(gossmap, a))
		return false;
	if (c->half[!route->direction].nodeidx
	    != gossmap_node_idx(gossmap, b))
		return false;

	return true;
}

/* route_can_carry disregards unless *both* dirs are enabled, so we use
 * a simpler variant here */
static bool route_can_carry_unless_disabled(const struct gossmap *map,
					    const struct gossmap_chan *c,
					    int dir,
					    struct amount_msat amount,
					    void *arg)
{
	if (!c->half[dir].enabled)
		return false;
	return route_can_carry_even_disabled(map, c, dir, amount, arg);
}

static void node_id_from_privkey(const struct privkey *p, struct node_id *id)
{
	struct pubkey k;
	pubkey_from_privkey(p, &k);
	node_id_from_pubkey(id, &k);
}

int main(int argc, char *argv[])
{
	common_setup(argv[0]);

	struct node_id a, b, c, d;
	struct gossmap_node *a_node, *b_node, *c_node, *d_node;
	struct privkey tmp;
	const struct dijkstra *dij;
	struct route_hop *route;
	int store_fd;
	struct gossmap *gossmap;
	const double riskfactor = 1.0;
	char gossip_version = 10;
	char *gossipfilename;

	chainparams = chainparams_for_network("regtest");

	store_fd = tmpdir_mkstemp(tmpctx, "run-route-gossipstore.XXXXXX", &gossipfilename);
	assert(write(store_fd, &gossip_version, sizeof(gossip_version))
	       == sizeof(gossip_version));
	gossmap = gossmap_load(tmpctx, gossipfilename, NULL);

	memset(&tmp, 'a', sizeof(tmp));
	node_id_from_privkey(&tmp, &a);
	memset(&tmp, 'b', sizeof(tmp));
	node_id_from_privkey(&tmp, &b);

	assert(!gossmap_refresh(gossmap, NULL));

	/* A<->B */
	add_connection(store_fd, &a, &b, 1, 1, 1);
	assert(gossmap_refresh(gossmap, NULL));

	a_node = gossmap_find_node(gossmap, &a);
	b_node = gossmap_find_node(gossmap, &b);
	dij = dijkstra(tmpctx, gossmap, b_node, AMOUNT_MSAT(1000), riskfactor,
		       route_can_carry_unless_disabled,
		       route_score_cheaper, NULL);

	route = route_from_dijkstra(tmpctx, gossmap, dij, a_node, AMOUNT_MSAT(1000), 10);
	assert(route);
	assert(tal_count(route) == 1);
	assert(amount_msat_eq(route[0].amount, AMOUNT_MSAT(1000)));
	assert(route[0].delay == 10);

	/* A<->B<->C */
	memset(&tmp, 'c', sizeof(tmp));
	node_id_from_privkey(&tmp, &c);
	add_connection(store_fd, &b, &c, 1, 1, 1);
	assert(gossmap_refresh(gossmap, NULL));

	/* These can theoretically change after refresh! */
	a_node = gossmap_find_node(gossmap, &a);
	b_node = gossmap_find_node(gossmap, &b);
	c_node = gossmap_find_node(gossmap, &c);
	dij = dijkstra(tmpctx, gossmap, c_node, AMOUNT_MSAT(1000), riskfactor,
		       route_can_carry_unless_disabled,
		       route_score_cheaper, NULL);
	route = route_from_dijkstra(tmpctx, gossmap, dij, a_node,
				    AMOUNT_MSAT(1000), 11);
	assert(route);
	assert(tal_count(route) == 2);
	assert(amount_msat_eq(route[1].amount, AMOUNT_MSAT(1000)));
	assert(route[1].delay == 11);
	assert(amount_msat_eq(route[0].amount, AMOUNT_MSAT(1001)));
	assert(route[0].delay == 12);

	/* A<->D<->C: Lower base, higher percentage. */
	memset(&tmp, 'd', sizeof(tmp));
	node_id_from_privkey(&tmp, &d);

	add_connection(store_fd, &a, &d, 0, 2, 1);
	add_connection(store_fd, &d, &c, 0, 2, 1);
	assert(gossmap_refresh(gossmap, NULL));

	/* These can theoretically change after refresh! */
	a_node = gossmap_find_node(gossmap, &a);
	b_node = gossmap_find_node(gossmap, &b);
	c_node = gossmap_find_node(gossmap, &c);
	d_node = gossmap_find_node(gossmap, &d);

	/* Will go via D for small amounts. */
	dij = dijkstra(tmpctx, gossmap, c_node, AMOUNT_MSAT(1000), riskfactor,
		       route_can_carry_unless_disabled,
		       route_score_cheaper, NULL);
	route = route_from_dijkstra(tmpctx, gossmap, dij, a_node,
				    AMOUNT_MSAT(1000), 12);

	assert(route);
	assert(tal_count(route) == 2);
	assert(channel_is_between(gossmap, &route[0], a_node, d_node));
	assert(channel_is_between(gossmap, &route[1], d_node, c_node));
	assert(amount_msat_eq(route[1].amount, AMOUNT_MSAT(1000)));
	assert(route[1].delay == 12);
	assert(amount_msat_eq(route[0].amount, AMOUNT_MSAT(1000)));
	assert(route[0].delay == 13);

	/* Will go via B for large amounts. */
	dij = dijkstra(tmpctx, gossmap, c_node, AMOUNT_MSAT(3000000), riskfactor,
		       route_can_carry_unless_disabled,
		       route_score_cheaper, NULL);
	route = route_from_dijkstra(tmpctx, gossmap, dij, a_node,
				    AMOUNT_MSAT(3000000), 13);
	assert(route);
	assert(tal_count(route) == 2);
	assert(channel_is_between(gossmap, &route[0], a_node, b_node));
	assert(channel_is_between(gossmap, &route[1], b_node, c_node));
	assert(amount_msat_eq(route[1].amount, AMOUNT_MSAT(3000000)));
	assert(route[1].delay == 13);
	assert(amount_msat_eq(route[0].amount, AMOUNT_MSAT(3000000 + 3 + 1)));
	assert(route[0].delay == 14);

	/* Make B->C inactive, force it back via D */
	update_connection(store_fd, &b, &c, 1, 1, 1, true);
	assert(gossmap_refresh(gossmap, NULL));

	/* These can theoretically change after refresh! */
	a_node = gossmap_find_node(gossmap, &a);
	b_node = gossmap_find_node(gossmap, &b);
	c_node = gossmap_find_node(gossmap, &c);
	d_node = gossmap_find_node(gossmap, &d);

	dij = dijkstra(tmpctx, gossmap, c_node, AMOUNT_MSAT(3000000), riskfactor,
		       route_can_carry_unless_disabled,
		       route_score_cheaper, NULL);
	route = route_from_dijkstra(tmpctx, gossmap, dij, a_node,
				    AMOUNT_MSAT(3000000), 14);
	assert(route);
	assert(tal_count(route) == 2);
	assert(channel_is_between(gossmap, &route[0], a_node, d_node));
	assert(channel_is_between(gossmap, &route[1], d_node, c_node));
	assert(amount_msat_eq(route[1].amount, AMOUNT_MSAT(3000000)));
	assert(route[1].delay == 14);
	assert(amount_msat_eq(route[0].amount, AMOUNT_MSAT(3000000 + 6)));
	assert(route[0].delay == 15);

	common_shutdown();
	return 0;
}
