#include "config.h"
#include "../bigsize.c"
#include "../blinding.c"
#include "../hmac.c"
#include <common/channel_id.h>
#include <common/setup.h>
#include <stdio.h>

#undef SUPERVERBOSE
#define SUPERVERBOSE printf
  #include "../blindedpath.c"

/* AUTOGENERATED MOCKS START */
/* Generated stub for amount_asset_is_main */
bool amount_asset_is_main(struct amount_asset *asset UNNEEDED)
{ fprintf(stderr, "amount_asset_is_main called!\n"); abort(); }
/* Generated stub for amount_asset_to_sat */
struct amount_sat amount_asset_to_sat(struct amount_asset *asset UNNEEDED)
{ fprintf(stderr, "amount_asset_to_sat called!\n"); abort(); }
/* Generated stub for amount_feerate */
 bool amount_feerate(u32 *feerate UNNEEDED, struct amount_sat fee UNNEEDED, size_t weight UNNEEDED)
{ fprintf(stderr, "amount_feerate called!\n"); abort(); }
/* Generated stub for amount_sat */
struct amount_sat amount_sat(u64 satoshis UNNEEDED)
{ fprintf(stderr, "amount_sat called!\n"); abort(); }
/* Generated stub for amount_sat_add */
 bool amount_sat_add(struct amount_sat *val UNNEEDED,
				       struct amount_sat a UNNEEDED,
				       struct amount_sat b UNNEEDED)
{ fprintf(stderr, "amount_sat_add called!\n"); abort(); }
/* Generated stub for amount_sat_eq */
bool amount_sat_eq(struct amount_sat a UNNEEDED, struct amount_sat b UNNEEDED)
{ fprintf(stderr, "amount_sat_eq called!\n"); abort(); }
/* Generated stub for amount_sat_greater_eq */
bool amount_sat_greater_eq(struct amount_sat a UNNEEDED, struct amount_sat b UNNEEDED)
{ fprintf(stderr, "amount_sat_greater_eq called!\n"); abort(); }
/* Generated stub for amount_sat_sub */
 bool amount_sat_sub(struct amount_sat *val UNNEEDED,
				       struct amount_sat a UNNEEDED,
				       struct amount_sat b UNNEEDED)
{ fprintf(stderr, "amount_sat_sub called!\n"); abort(); }
/* Generated stub for amount_sat_to_asset */
struct amount_asset amount_sat_to_asset(struct amount_sat *sat UNNEEDED, const u8 *asset UNNEEDED)
{ fprintf(stderr, "amount_sat_to_asset called!\n"); abort(); }
/* Generated stub for amount_tx_fee */
struct amount_sat amount_tx_fee(u32 fee_per_kw UNNEEDED, size_t weight UNNEEDED)
{ fprintf(stderr, "amount_tx_fee called!\n"); abort(); }
/* Generated stub for fromwire_amount_msat */
struct amount_msat fromwire_amount_msat(const u8 **cursor UNNEEDED, size_t *max UNNEEDED)
{ fprintf(stderr, "fromwire_amount_msat called!\n"); abort(); }
/* Generated stub for fromwire_amount_sat */
struct amount_sat fromwire_amount_sat(const u8 **cursor UNNEEDED, size_t *max UNNEEDED)
{ fprintf(stderr, "fromwire_amount_sat called!\n"); abort(); }
/* Generated stub for fromwire_channel_id */
bool fromwire_channel_id(const u8 **cursor UNNEEDED, size_t *max UNNEEDED,
			 struct channel_id *channel_id UNNEEDED)
{ fprintf(stderr, "fromwire_channel_id called!\n"); abort(); }
/* Generated stub for fromwire_node_id */
void fromwire_node_id(const u8 **cursor UNNEEDED, size_t *max UNNEEDED, struct node_id *id UNNEEDED)
{ fprintf(stderr, "fromwire_node_id called!\n"); abort(); }
/* Generated stub for fromwire_sciddir_or_pubkey */
void fromwire_sciddir_or_pubkey(const u8 **cursor UNNEEDED, size_t *max UNNEEDED,
				struct sciddir_or_pubkey *sciddpk UNNEEDED)
{ fprintf(stderr, "fromwire_sciddir_or_pubkey called!\n"); abort(); }
/* Generated stub for towire_amount_msat */
void towire_amount_msat(u8 **pptr UNNEEDED, const struct amount_msat msat UNNEEDED)
{ fprintf(stderr, "towire_amount_msat called!\n"); abort(); }
/* Generated stub for towire_amount_sat */
void towire_amount_sat(u8 **pptr UNNEEDED, const struct amount_sat sat UNNEEDED)
{ fprintf(stderr, "towire_amount_sat called!\n"); abort(); }
/* Generated stub for towire_channel_id */
void towire_channel_id(u8 **pptr UNNEEDED, const struct channel_id *channel_id UNNEEDED)
{ fprintf(stderr, "towire_channel_id called!\n"); abort(); }
/* Generated stub for towire_node_id */
void towire_node_id(u8 **pptr UNNEEDED, const struct node_id *id UNNEEDED)
{ fprintf(stderr, "towire_node_id called!\n"); abort(); }
/* Generated stub for towire_sciddir_or_pubkey */
void towire_sciddir_or_pubkey(u8 **pptr UNNEEDED,
			      const struct sciddir_or_pubkey *sciddpk UNNEEDED)
{ fprintf(stderr, "towire_sciddir_or_pubkey called!\n"); abort(); }
/* AUTOGENERATED MOCKS END */

static void json_strfield(const char *name, const char *val)
{
	printf("\t\"%s\": \"%s\",\n", name, val);
}

/* Updated each time, as we pretend to be Alice, Bob, Carol */
static const struct privkey *mykey;

static void test_ecdh(const struct pubkey *point, struct secret *ss)
{
	if (secp256k1_ecdh(secp256k1_ctx, ss->data, &point->pubkey,
			   mykey->secret.data, NULL, NULL) != 1)
		abort();
}

static void test_decrypt(const struct pubkey *blinding,
			 const u8 *enctlv,
			 const struct privkey *me,
			 const struct pubkey *expected_next_node,
			 const struct privkey *expected_next_blinding_priv)
{
	struct pubkey expected_next_blinding, dummy, next_blinding;
	struct secret ss;
	struct tlv_encrypted_data_tlv *enc;

	/* We don't actually have an onion, so we put some dummy */
	pubkey_from_privkey(me, &dummy);

	mykey = me;
	assert(unblind_onion(blinding, test_ecdh, &dummy, &ss));
	enc = decrypt_encrypted_data(tmpctx, blinding, &ss, enctlv);
	assert(enc);

	pubkey_from_privkey(expected_next_blinding_priv, &expected_next_blinding);
	blindedpath_next_blinding(enc, blinding, &ss, &next_blinding);
	assert(pubkey_eq(&next_blinding, &expected_next_blinding));
	assert(pubkey_eq(enc->next_node_id, expected_next_node));
}

static void test_final_decrypt(const struct pubkey *blinding,
			       const u8 *enctlv,
			       const struct privkey *me,
			       const struct pubkey *expected_alias,
			       const struct secret *expected_self_id)
{
	struct pubkey my_pubkey, dummy, alias;
	struct secret ss;
	struct tlv_encrypted_data_tlv *enc;

	/* We don't actually have an onion, so we put some dummy */
	pubkey_from_privkey(me, &dummy);

	mykey = me;
	pubkey_from_privkey(me, &my_pubkey);
	assert(unblind_onion(blinding, test_ecdh, &dummy, &ss));
	enc = decrypt_encrypted_data(tmpctx, blinding, &ss, enctlv);
	assert(enc);
	assert(blindedpath_get_alias(&ss, &my_pubkey, &alias));

	assert(pubkey_eq(&alias, expected_alias));
	assert(memeq(enc->path_id, tal_bytelen(enc->path_id), expected_self_id,
		     sizeof(*expected_self_id)));
}

int main(int argc, char *argv[])
{
	struct privkey alice, bob, carol, dave, blinding, override_blinding;
	struct pubkey alice_id, bob_id, carol_id, dave_id, blinding_pub, override_blinding_pub, alias;
	struct secret self_id;
	u8 *enctlv;
	struct tlv_encrypted_data_tlv *tlv;

	common_setup(argv[0]);

	memset(&alice, 'A', sizeof(alice));
	memset(&bob, 'B', sizeof(bob));
	memset(&carol, 'C', sizeof(carol));
	memset(&dave, 'D', sizeof(dave));
	pubkey_from_privkey(&alice, &alice_id);
	pubkey_from_privkey(&bob, &bob_id);
	pubkey_from_privkey(&carol, &carol_id);
	pubkey_from_privkey(&dave, &dave_id);

	memset(&blinding, 5, sizeof(blinding));
	pubkey_from_privkey(&blinding, &blinding_pub);

	/* We output the JSON test vectors. */
	printf("[{");
	json_strfield("test name", "Simple encrypted_recipient_data for Alice, next is Bob");
	json_strfield("node_privkey",
		      fmt_privkey(tmpctx, &alice));
	json_strfield("node_id",
		      fmt_pubkey(tmpctx, &alice_id));
	json_strfield("blinding_secret",
		      fmt_privkey(tmpctx, &blinding));
	json_strfield("blinding",
		      fmt_pubkey(tmpctx, &blinding_pub));
	printf("\t\"encrypted_data_tlv\": {\n"
	       "\t\t\"next_node_id\": \"%s\"\n"
	       "\t},\n",
	       fmt_pubkey(tmpctx, &bob_id));

	tlv = tlv_encrypted_data_tlv_new(tmpctx);
	tlv->next_node_id = &bob_id;
	enctlv = encrypt_tlv_encrypted_data(tmpctx, &blinding, &alice_id, tlv,
					    &blinding, &alias);
	printf("\t\"encrypted_recipient_data_hex\": \"%s\"\n"
	       "},\n",
	       tal_hex(tmpctx, enctlv));

	test_decrypt(&blinding_pub, enctlv, &alice, &bob_id, &blinding);

	pubkey_from_privkey(&blinding, &blinding_pub);
	memset(&override_blinding, 7, sizeof(override_blinding));
	pubkey_from_privkey(&override_blinding, &override_blinding_pub);

	printf("{");
	json_strfield("test name",
		      "Blinding-key-override encrypted_recipient_data for Bob, next is Carol");
	json_strfield("node_privkey",
		      fmt_privkey(tmpctx, &bob));
	json_strfield("node_id",
		      fmt_pubkey(tmpctx, &bob_id));
	json_strfield("blinding_secret",
		      fmt_privkey(tmpctx, &blinding));
	json_strfield("blinding",
		      fmt_pubkey(tmpctx, &blinding_pub));
	printf("\t\"encrypted_data_tlv\": {\n"
	       "\t\t\"next_node_id\": \"%s\",\n"
	       "\t\t\"blinding\": \"%s\"\n"
	       "\t},\n",
	       fmt_pubkey(tmpctx, &carol_id),
	       fmt_privkey(tmpctx, &override_blinding));

	tlv = tlv_encrypted_data_tlv_new(tmpctx);
	tlv->next_node_id = &carol_id;
	tlv->next_blinding_override = &override_blinding_pub;
	enctlv = encrypt_tlv_encrypted_data(tmpctx, &blinding, &bob_id, tlv,
					    &blinding, &alias);
	printf("\t\"encrypted_recipient_data_hex\": \"%s\"\n"
	       "},\n",
	       tal_hex(tmpctx, enctlv));

	test_decrypt(&blinding_pub, enctlv, &bob, &carol_id, &override_blinding);

	/* That replaced the blinding */
	blinding = override_blinding;
	blinding_pub = override_blinding_pub;

	printf("{");
	json_strfield("test name", "Padded encrypted_recipient_data for Carol, next is Dave");
	json_strfield("node_privkey",
		      fmt_privkey(tmpctx, &carol));
	json_strfield("node_id",
		      fmt_pubkey(tmpctx, &carol_id));
	json_strfield("blinding_secret",
		      fmt_privkey(tmpctx, &blinding));
	json_strfield("blinding",
		      fmt_pubkey(tmpctx, &blinding_pub));
	printf("\t\"encrypted_data_tlv\": {\n"
	       "\t\t\"next_node_id\": \"%s\",\n"
	       "\t\t\"padding\": \"%s\"\n"
	       "\t},\n",
	       fmt_pubkey(tmpctx, &dave_id),
	       tal_hex(tmpctx, tal_arrz(tmpctx, u8, 35)));

	tlv = tlv_encrypted_data_tlv_new(tmpctx);
	tlv->padding = tal_arrz(tlv, u8, 35);
	tlv->next_node_id = &dave_id;
	enctlv = encrypt_tlv_encrypted_data(tmpctx, &blinding, &carol_id, tlv,
					    &blinding, &alias);
	printf("\t\"encrypted_recipient_data_hex\": \"%s\"\n"
	       "},\n",
	       tal_hex(tmpctx, enctlv));

	test_decrypt(&blinding_pub, enctlv, &carol, &dave_id, &blinding);

	for (size_t i = 0; i < sizeof(self_id); i++)
		self_id.data[i] = i+1;
	printf("{");
	json_strfield("test name", "Final enctlv for Dave");
	json_strfield("node_privkey",
		      fmt_privkey(tmpctx, &dave));
	json_strfield("node_id",
		      fmt_pubkey(tmpctx, &dave_id));
	json_strfield("blinding_secret",
		      fmt_privkey(tmpctx, &blinding));
	json_strfield("blinding",
		      fmt_pubkey(tmpctx, &blinding_pub));
	printf("\t\"encrypted_data_tlv\": {\n"
	       "\t\t\"self_id\": \"%s\"\n"
	       "\t},\n",
	       fmt_secret(tmpctx, &self_id));

	tlv = tlv_encrypted_data_tlv_new(tmpctx);
	tlv->path_id = tal_dup_arr(tlv, u8,
				   self_id.data, ARRAY_SIZE(self_id.data), 0);
	enctlv = encrypt_tlv_encrypted_data(tmpctx, &blinding, &dave_id, tlv,
					    NULL, &alias);

	printf("\t\"encrypted_recipient_data_hex\": \"%s\"\n",
	       tal_hex(tmpctx, enctlv));

	printf("}]\n");
	pubkey_from_privkey(&blinding, &blinding_pub);

	test_final_decrypt(&blinding_pub, enctlv, &dave, &alias, &self_id);
	common_shutdown();
}